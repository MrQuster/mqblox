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
#include <cmath>
#include <cstdio>
#include <vector>

namespace mqblox {

QImage Ton618GlWidget::buildCrystalFrostedImage(const QImage &src, int radius) {
    if (src.isNull() || radius <= 1) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp(lum + ((r - lum) * 165) / 100 + 6, 0, 255);
            g = std::clamp(lum + ((g - lum) * 155) / 100 + 5, 0, 255);
            b = std::clamp(lum + ((b - lum) * 170) / 100 + 8, 0, 255);
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

void Ton618GlWidget::buildAdaptiveColorMap(const QImage &srcSmall,
                                           QImage &outMap,
                                           QColor &outPrimary,
                                           QColor &outSecondary) {
    QImage cmap = srcSmall.scaled(64, 36, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                      .convertToFormat(QImage::Format_RGB32);
    const int w = cmap.width(), h = cmap.height();

    double rAcc1 = 0, gAcc1 = 0, bAcc1 = 0, wAcc1 = 0.001;
    double rAcc2 = 0, gAcc2 = 0, bAcc2 = 0, wAcc2 = 0.001;

    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(cmap.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int cMax = std::max({r, g, b}), cMin = std::min({r, g, b});
            double sat = (cMax - cMin) + 18.0;
            double lum = (r + g + b) / 3.0;
            double weight = sat * std::max(10.0, lum);
            if (x < w / 2) {
                rAcc1 += r * weight; gAcc1 += g * weight; bAcc1 += b * weight; wAcc1 += weight;
            } else {
                rAcc2 += r * weight; gAcc2 += g * weight; bAcc2 += b * weight; wAcc2 += weight;
            }
        }
    }

    auto normalizeVibrant = [](double r, double g, double b) -> QColor {
        QColor c(std::clamp(int(r), 0, 255), std::clamp(int(g), 0, 255), std::clamp(int(b), 0, 255));
        float hf = 0.08f, sf = 0.7f, lf = 0.68f;
        c.getHslF(&hf, &sf, &lf);
        if (hf < 0.0f) hf = 0.08f;
        sf = std::clamp(sf * 1.45f + 0.25f, 0.55f, 0.96f);
        lf = std::clamp(lf * 1.15f + 0.28f, 0.62f, 0.80f);
        return QColor::fromHslF(hf, sf, lf);
    };

    outPrimary = normalizeVibrant(rAcc1 / wAcc1, gAcc1 / wAcc1, bAcc1 / wAcc1);
    outSecondary = normalizeVibrant(rAcc2 / wAcc2, gAcc2 / wAcc2, bAcc2 / wAcc2);

    // Обогащаем локальную карту 64x36, чтобы рамки в любой точке экрана светились оттенком фона под ними
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(cmap.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QColor c(qRed(line[x]), qGreen(line[x]), qBlue(line[x]));
            float hf = 0.0f, sf = 0.0f, lf = 0.0f;
            c.getHslF(&hf, &sf, &lf);

            if (lf < 0.12f || sf < 0.10f || hf < 0.0f) {
                // На тёмных участках космоса плавно подмешиваем доминирующий тон плазмы текущего кадра
                double mix = double(x) / std::max(1, w - 1);
                int mr = int(outPrimary.red() * (1.0 - mix) + outSecondary.red() * mix);
                int mg = int(outPrimary.green() * (1.0 - mix) + outSecondary.green() * mix);
                int mb = int(outPrimary.blue() * (1.0 - mix) + outSecondary.blue() * mix);
                line[x] = qRgb((c.red() + mr * 2) / 3, (c.green() + mg * 2) / 3, (c.blue() + mb * 2) / 3);
            } else {
                sf = std::clamp(sf * 1.38f + 0.22f, 0.50f, 0.96f);
                lf = std::clamp(0.54f + lf * 0.34f, 0.60f, 0.84f);
                QColor vc = QColor::fromHslF(hf, sf, lf);
                line[x] = qRgb(vc.red(), vc.green(), vc.blue());
            }
        }
    }
    outMap = cmap;
}

QColor Ton618GlWidget::sampleAdaptiveColor(const QWidget *caller, const QPointF &localPos) const {
    if (m_colorMap.isNull() || width() <= 1 || height() <= 1) {
        return m_ambientPrimary;
    }
    QPoint pt = caller ? caller->mapTo(const_cast<Ton618GlWidget *>(this), localPos.toPoint())
                       : localPos.toPoint();
    int cx = std::clamp(pt.x() * m_colorMap.width() / std::max(1, width()), 0, m_colorMap.width() - 1);
    int cy = std::clamp(pt.y() * m_colorMap.height() / std::max(1, height()), 0, m_colorMap.height() - 1);
    return m_colorMap.pixelColor(cx, cy);
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
    constexpr auto frameInterval = std::chrono::microseconds(33333);

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

            QImage sharpScaled = rawFrame.scaled(tw, th, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int offX = std::max(0, (sharpScaled.width() - tw) / 2);
            int offY = std::max(0, (sharpScaled.height() - th) / 2);
            QImage sharpFinal = sharpScaled.copy(offX, offY, tw, th);

            QImage small = sharpFinal.scaled(240, 135, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            QImage blurredSmall = buildCrystalFrostedImage(small, 4);
            QImage frostedFinal = blurredSmall.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            QImage cmap;
            QColor amb1, amb2;
            buildAdaptiveColorMap(blurredSmall, cmap, amb1, amb2);

            m_frameQueued = true;
            QMetaObject::invokeMethod(this, [this, sharpFinal, frostedFinal, cmap, amb1, amb2]() {
                m_scaledSharp = QPixmap::fromImage(sharpFinal);
                m_scaledFrosted = QPixmap::fromImage(frostedFinal);
                m_colorMap = cmap;
                m_ambientPrimary = amb1;
                m_ambientSecondary = amb2;
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
    buildAdaptiveColorMap(blurredSmall, m_colorMap, m_ambientPrimary, m_ambientSecondary);
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

    if (!m_scaledSharp.isNull()) {
        if (m_scaledSharp.size() == size()) {
            p.drawPixmap(0, 0, m_scaledSharp);
        } else {
            p.drawPixmap(rect(), m_scaledSharp);
        }
    } else {
        p.fillRect(rect(), QColor(3, 4, 9));
    }

    // Внешняя тонкая рамка самого окна тоже плавно переливается цветами заднего фона!
    QLinearGradient winRim(0, 0, w, h);
    QColor c1 = m_ambientPrimary, c2 = m_ambientSecondary;
    c1.setAlpha(125);
    c2.setAlpha(115);
    winRim.setColorAt(0.0, QColor(255, 255, 255, 135));
    winRim.setColorAt(0.5, c1);
    winRim.setColorAt(1.0, c2);
    p.setPen(QPen(QBrush(winRim), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(1, 1, w - 2, h - 2, 20, 20);
}

} // namespace mqblox
