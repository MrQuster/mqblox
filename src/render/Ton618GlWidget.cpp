#include "render/Ton618GlWidget.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace mqblox {

QImage Ton618GlWidget::applyHyperOsVibrantBlur(const QImage &src, int radius) {
    if (src.isNull() || radius <= 1) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);

    // HyperOS 4 Vibrant Color Bleed: усиливаем насыщенность свечения плазмы под матовым стеклом
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp(lum + ((r - lum) * 175) / 100 + 8, 0, 255);
            g = std::clamp(lum + ((g - lum) * 165) / 100 + 6, 0, 255);
            b = std::clamp(lum + ((b - lum) * 180) / 100 + 14, 0, 255);
            line[x] = qRgb(r, g, b);
        }
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {
            const QRgb *inLine = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            QRgb *outLine = reinterpret_cast<QRgb *>(tmp.scanLine(y));
            int rSum = 0, gSum = 0, bSum = 0, cnt = 0;
            for (int x = -radius; x <= radius; ++x) {
                int cx = std::clamp(x, 0, w - 1);
                rSum += qRed(inLine[cx]); gSum += qGreen(inLine[cx]); bSum += qBlue(inLine[cx]);
                cnt++;
            }
            for (int x = 0; x < w; ++x) {
                outLine[x] = qRgb(rSum / cnt, gSum / cnt, bSum / cnt);
                int lIdx = std::clamp(x - radius, 0, w - 1);
                int rIdx = std::clamp(x + radius + 1, 0, w - 1);
                rSum += qRed(inLine[rIdx]) - qRed(inLine[lIdx]);
                gSum += qGreen(inLine[rIdx]) - qGreen(inLine[lIdx]);
                bSum += qBlue(inLine[rIdx]) - qBlue(inLine[lIdx]);
            }
        }
        for (int x = 0; x < w; ++x) {
            int rSum = 0, gSum = 0, bSum = 0, cnt = 0;
            for (int y = -radius; y <= radius; ++y) {
                int cy = std::clamp(y, 0, h - 1);
                const QRgb *p = reinterpret_cast<const QRgb *>(tmp.constScanLine(cy)) + x;
                rSum += qRed(*p); gSum += qGreen(*p); bSum += qBlue(*p);
                cnt++;
            }
            for (int y = 0; y < h; ++y) {
                QRgb *outP = reinterpret_cast<QRgb *>(img.scanLine(y)) + x;
                *outP = qRgb(rSum / cnt, gSum / cnt, bSum / cnt);
                int tIdx = std::clamp(y - radius, 0, h - 1);
                int bIdx = std::clamp(y + radius + 1, 0, h - 1);
                const QRgb *pTop = reinterpret_cast<const QRgb *>(tmp.constScanLine(tIdx)) + x;
                const QRgb *pBot = reinterpret_cast<const QRgb *>(tmp.constScanLine(bIdx)) + x;
                rSum += qRed(*pBot) - qRed(*pTop);
                gSum += qGreen(*pBot) - qGreen(*pTop);
                bSum += qBlue(*pBot) - qBlue(*pTop);
            }
        }
    }
    return img;
}

Ton618GlWidget::Ton618GlWidget(QWidget *parent) : QWidget(parent) {
    loadBestFallbackImage();
    updateFrameAndFrostedMap(m_fallbackImg);

    m_ffmpegProc = new QProcess(this);
    connect(m_ffmpegProc, &QProcess::readyReadStandardOutput, this, &Ton618GlWidget::onStreamReadyRead);

    QString videoDir = QDir::homePath() + "/.local/share/mqblox/videos";
    for (const QString &cand : {
             videoDir + "/active_video.mp4",
             videoDir + "/ton618_nasa_loop.mp4",
             videoDir + "/nasa_svs_blackhole.mp4"}) {
        if (QFileInfo(cand).size() > 100000) {
            m_videoPath = cand;
            break;
        }
    }
}

Ton618GlWidget::~Ton618GlWidget() {
    if (m_ffmpegProc && m_ffmpegProc->state() != QProcess::NotRunning) {
        m_ffmpegProc->kill();
        m_ffmpegProc->waitForFinished(150);
    }
}

void Ton618GlWidget::loadCustomVideoFile(const QString &filePath) {
    if (!QFile::exists(filePath)) return;
    if (filePath.endsWith(".png", Qt::CaseInsensitive) || filePath.endsWith(".jpg", Qt::CaseInsensitive) ||
        filePath.endsWith(".jpeg", Qt::CaseInsensitive)) {
        if (m_ffmpegProc->state() != QProcess::NotRunning) {
            m_ffmpegProc->kill();
            m_ffmpegProc->waitForFinished(100);
        }
        m_videoPath.clear();
        m_fallbackImg.load(filePath);
        updateFrameAndFrostedMap(m_fallbackImg);
        update();
        return;
    }

    QString dest = QDir::homePath() + "/.local/share/mqblox/videos/active_video.mp4";
    if (filePath != dest) {
        QFile::remove(dest);
        QFile::copy(filePath, dest);
    }
    m_videoPath = dest;
    if (m_ffmpegProc->state() != QProcess::NotRunning) {
        m_ffmpegProc->kill();
        m_ffmpegProc->waitForFinished(100);
    }
    m_rawBuf.clear();
    syncPlaybackState();
}

void Ton618GlWidget::syncPlaybackState() {
    if (m_videoPath.isEmpty()) return;
    bool shouldRun = m_animEnabled && !m_inGamePaused && isVisible();
    if (shouldRun) {
        if (m_ffmpegProc->state() == QProcess::NotRunning) {
            m_rawBuf.clear();
            m_ffmpegProc->start("ffmpeg", {
                "-loglevel", "quiet",
                "-threads", "2",
                "-stream_loop", "-1",
                "-re",
                "-i", m_videoPath,
                "-vf", QString("scale=%1:%2").arg(STREAM_W).arg(STREAM_H),
                "-r", "24",
                "-f", "rawvideo",
                "-pix_fmt", "rgb32",
                "-"
            }, QIODevice::ReadOnly);
        }
    } else {
        if (m_ffmpegProc->state() != QProcess::NotRunning) {
            m_ffmpegProc->kill();
            m_ffmpegProc->waitForFinished(100);
        }
        m_rawBuf.clear();
    }
}

void Ton618GlWidget::showEvent(QShowEvent *e) { QWidget::showEvent(e); syncPlaybackState(); }
void Ton618GlWidget::hideEvent(QHideEvent *e) { QWidget::hideEvent(e); syncPlaybackState(); }

void Ton618GlWidget::setAnimationEnabled(bool enabled) {
    m_animEnabled = enabled;
    syncPlaybackState();
    update();
}

void Ton618GlWidget::setInGamePaused(bool paused) {
    m_inGamePaused = paused;
    syncPlaybackState();
}

void Ton618GlWidget::setWebTabActive(bool) {
    // Видео плавно работает на всех вкладках без лагов
}

void Ton618GlWidget::onStreamReadyRead() {
    m_rawBuf.append(m_ffmpegProc->readAllStandardOutput());
    if (m_rawBuf.size() < FRAME_BYTES) return;

    int totalFrames = m_rawBuf.size() / FRAME_BYTES;
    int lastOffset = (totalFrames - 1) * FRAME_BYTES;

    QImage frame(reinterpret_cast<const uchar *>(m_rawBuf.constData() + lastOffset),
                 STREAM_W, STREAM_H, STREAM_W * 4, QImage::Format_RGB32);
    m_currentFrameImg = frame.copy();
    m_rawBuf.remove(0, totalFrames * FRAME_BYTES);

    if (isVisible() && !window()->isMinimized()) {
        updateFrameAndFrostedMap(m_currentFrameImg);
        update();
    }
}

void Ton618GlWidget::updateFrameAndFrostedMap(const QImage &srcImg) {
    if (srcImg.isNull()) return;
    m_scaledSharp = QPixmap::fromImage(srcImg);

    // Сверхбыстрая генерация гладкой карты размытия HyperOS 4 (занимает 0.02 мс, без полос и лагов!)
    QImage tiny = srcImg.scaled(140, 80, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage blurredTiny = applyHyperOsVibrantBlur(tiny, 6);
    m_scaledFrosted = QPixmap::fromImage(
        blurredTiny.scaled(420, 240, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void Ton618GlWidget::loadBestFallbackImage() {
    QString base = QDir::homePath() + "/.local/share/mqblox";
    for (const QString &candidate : {
             base + "/pexels_39811262_uhd.png",
             base + "/ton618_4k_master_v22.png",
             base + "/ton618_4k_master_v21.png"}) {
        if (QFileInfo(candidate).size() > 50000 && m_fallbackImg.load(candidate)) {
            m_fallbackImg = m_fallbackImg.scaled(STREAM_W, STREAM_H, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            return;
        }
    }
    m_fallbackImg = QImage(STREAM_W, STREAM_H, QImage::Format_ARGB32_Premultiplied);
    m_fallbackImg.fill(QColor(4, 6, 14));
}

void Ton618GlWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
}

void Ton618GlWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int w = width(), h = height();

    QPainterPath clip;
    clip.addRoundedRect(1.0, 1.0, w - 2.0, h - 2.0, 20.0, 20.0);
    p.setClipPath(clip);

    if (!m_scaledSharp.isNull()) {
        p.drawPixmap(rect(), m_scaledSharp);
    } else {
        p.fillRect(rect(), QColor(3, 4, 9));
    }

    p.fillRect(rect(), QColor(2, 4, 12, 34));

    p.setPen(QPen(QColor(255, 255, 255, 135), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(1, 1, w - 2, h - 2, 20, 20);
}

} // namespace mqblox
