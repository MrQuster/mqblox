#include "render/Ton618GlWidget.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace mqblox {

QImage Ton618GlWidget::buildCrystalFrostedImage(const QImage &src, int radius) {
    if (src.isNull() || radius <= 1) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);

    // HyperOS 4 Crystal Glass: усиливаем яркость и насыщенность свечения чёрной дыры под полупрозрачным стеклом
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp(lum + ((r - lum) * 165) / 100 + 10, 0, 255);
            g = std::clamp(lum + ((g - lum) * 155) / 100 + 8, 0, 255);
            b = std::clamp(lum + ((b - lum) * 170) / 100 + 14, 0, 255);
            line[x] = qRgb(r, g, b);
        }
    }

    for (int pass = 0; pass < 2; ++pass) {
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
    updateStaticCaches();

    QString videoDir = QDir::homePath() + "/.local/share/mqblox/videos";
    for (const QString &cand : {
             videoDir + "/smooth_loop.mp4",
             videoDir + "/active_video.mp4",
             videoDir + "/ton618_nasa_loop.mp4"}) {
        if (QFileInfo(cand).size() > 100000) {
            m_videoPath = cand;
            break;
        }
    }
    startDecoderThread();
}

Ton618GlWidget::~Ton618GlWidget() {
    stopDecoderThread();
}

void Ton618GlWidget::startDecoderThread() {
    stopDecoderThread();
    if (m_videoPath.isEmpty()) return;
    m_stopWorker = false;
    m_worker = std::thread(&Ton618GlWidget::decoderLoop, this);
}

void Ton618GlWidget::stopDecoderThread() {
    m_stopWorker = true;
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void Ton618GlWidget::decoderLoop() {
    QString path;
    {
        std::lock_guard<std::mutex> lk(m_pathMutex);
        path = m_videoPath;
    }
    if (path.isEmpty() || !QFile::exists(path)) return;

    QString cmd = QString("ffmpeg -loglevel quiet -threads 2 -stream_loop -1 -i \"%1\" "
                          "-vf scale=%2:%3 -r 30 -f rawvideo -pix_fmt rgb32 -")
                      .arg(path)
                      .arg(STREAM_W)
                      .arg(STREAM_H);

    FILE *pipe = popen(cmd.toUtf8().constData(), "r");
    if (!pipe) return;

    std::vector<uchar> buf(FRAME_BYTES);
    auto nextTick = std::chrono::steady_clock::now();
    constexpr auto frameInterval = std::chrono::microseconds(33333); // Точные 30.0 FPS

    while (!m_stopWorker) {
        if (m_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            nextTick = std::chrono::steady_clock::now();
            continue;
        }

        size_t readBytes = 0;
        while (readBytes < FRAME_BYTES && !m_stopWorker) {
            size_t n = std::fread(buf.data() + readBytes, 1, FRAME_BYTES - readBytes, pipe);
            if (n == 0) break;
            readBytes += n;
        }
        if (readBytes < FRAME_BYTES || m_stopWorker) break;

        int tw = std::max(320, m_targetW.load());
        int th = std::max(240, m_targetH.load());

        if (!m_frameQueued.load()) {
            QImage rawFrame(buf.data(), STREAM_W, STREAM_H, STREAM_W * 4, QImage::Format_RGB32);

            // 1. Масштабируем чёткий кадр заднего фона прямо в фоновом потоке
            QImage sharpScaled = rawFrame.scaled(tw, th, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int offX = std::max(0, (sharpScaled.width() - tw) / 2);
            int offY = std::max(0, (sharpScaled.height() - th) / 2);
            QImage sharpFinal = sharpScaled.copy(offX, offY, tw, th);

            // 2. Строим кристально-полупрозрачную карту преломления Liquid Glass (умеренное размытие, чтобы сквозь карточки читалось движение плазмы!)
            QImage small = sharpFinal.scaled(240, 135, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            QImage blurredSmall = buildCrystalFrostedImage(small, 4);
            QImage frostedFinal = blurredSmall.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            m_frameQueued = true;
            QMetaObject::invokeMethod(this, [this, sharpFinal, frostedFinal]() {
                m_scaledSharp = QPixmap::fromImage(sharpFinal);
                m_scaledFrosted = QPixmap::fromImage(frostedFinal);
                m_frameQueued = false;
                if (isVisible() && !window()->isMinimized()) {
                    update();
                }
            }, Qt::QueuedConnection);
        }

        nextTick += frameInterval;
        auto now = std::chrono::steady_clock::now();
        if (nextTick > now) {
            while (std::chrono::steady_clock::now() < nextTick && !m_stopWorker) {
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
        } else {
            nextTick = now;
        }
    }

    pclose(pipe);
}

void Ton618GlWidget::loadCustomVideoFile(const QString &filePath) {
    if (!QFile::exists(filePath)) return;
    stopDecoderThread();

    if (filePath.endsWith(".png", Qt::CaseInsensitive) || filePath.endsWith(".jpg", Qt::CaseInsensitive) ||
        filePath.endsWith(".jpeg", Qt::CaseInsensitive)) {
        {
            std::lock_guard<std::mutex> lk(m_pathMutex);
            m_videoPath.clear();
        }
        m_fallbackImg.load(filePath);
        updateStaticCaches();
        update();
        return;
    }

    QString dest = QDir::homePath() + "/.local/share/mqblox/videos/smooth_loop.mp4";
    QProcess::execute("ffmpeg", {
        "-loglevel", "error", "-y",
        "-i", filePath,
        "-vf", "scale=854:480:flags=lanczos,fps=30",
        "-c:v", "libx264", "-preset", "ultrafast", "-crf", "16", "-g", "15",
        "-pix_fmt", "yuv420p", "-an", dest
    });
    {
        std::lock_guard<std::mutex> lk(m_pathMutex);
        m_videoPath = dest;
    }
    startDecoderThread();
}

void Ton618GlWidget::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    m_paused = (!m_animEnabled || m_inGamePaused);
}

void Ton618GlWidget::hideEvent(QHideEvent *e) {
    QWidget::hideEvent(e);
    m_paused = true;
}

void Ton618GlWidget::setAnimationEnabled(bool enabled) {
    m_animEnabled = enabled;
    m_paused = (!m_animEnabled || m_inGamePaused || !isVisible());
    update();
}

void Ton618GlWidget::setInGamePaused(bool paused) {
    m_inGamePaused = paused;
    m_paused = (!m_animEnabled || m_inGamePaused || !isVisible());
}

void Ton618GlWidget::setWebTabActive(bool) {}

void Ton618GlWidget::loadBestFallbackImage() {
    QString base = QDir::homePath() + "/.local/share/mqblox";
    for (const QString &candidate : {
             base + "/pexels_39811262_uhd.png",
             base + "/ton618_4k_master_v22.png",
             base + "/ton618_4k_master_v21.png"}) {
        if (QFileInfo(candidate).size() > 50000 && m_fallbackImg.load(candidate)) {
            return;
        }
    }
    m_fallbackImg = QImage(STREAM_W, STREAM_H, QImage::Format_RGB32);
    m_fallbackImg.fill(QColor(4, 6, 14));
}

void Ton618GlWidget::updateStaticCaches() {
    if (width() <= 10 || height() <= 10 || m_fallbackImg.isNull()) return;
    QImage sharpScaled = m_fallbackImg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int offX = std::max(0, (sharpScaled.width() - width()) / 2);
    int offY = std::max(0, (sharpScaled.height() - height()) / 2);
    QImage cropped = sharpScaled.copy(offX, offY, width(), height());
    m_scaledSharp = QPixmap::fromImage(cropped);

    QImage small = cropped.scaled(240, 135, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage blurredSmall = buildCrystalFrostedImage(small, 4);
    m_scaledFrosted = QPixmap::fromImage(blurredSmall.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void Ton618GlWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    m_targetW = std::max(320, width());
    m_targetH = std::max(240, height());
    if (m_scaledSharp.isNull()) {
        updateStaticCaches();
    }
}

void Ton618GlWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int w = width(), h = height();

    QPainterPath clip;
    clip.addRoundedRect(1.0, 1.0, w - 2.0, h - 2.0, 20.0, 20.0);
    p.setClipPath(clip);

    // 100% НЕПРОЗРАЧНЫЙ И ЯРКИЙ ЗАДНИЙ ФОН ВИДЕО (без тёмной пелены!)
    if (!m_scaledSharp.isNull()) {
        if (m_scaledSharp.size() == size()) {
            p.drawPixmap(0, 0, m_scaledSharp);
        } else {
            p.drawPixmap(rect(), m_scaledSharp);
        }
    } else {
        p.fillRect(rect(), QColor(3, 4, 9));
    }

    p.setPen(QPen(QColor(255, 255, 255, 145), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(1, 1, w - 2, h - 2, 20, 20);
}

} // namespace mqblox
