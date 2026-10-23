#include <QGuiApplication>
#include <QStyleHints>
#include "render/Ton618GlWidget.hpp"
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLinearGradient>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProcess>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace mqblox {

QColor Ton618GlWidget::fixedAccentColor() const {
    int preset = m_accentPreset.load();
    if (preset == 4) {
        std::lock_guard<std::mutex> lk(m_visMutex);
        if (m_vis.customAccent.isValid()) return m_vis.customAccent;
    }
    switch (preset) {
        case 1: return QColor(251, 191, 36);  // Золото TON 618
        case 2: return QColor(52, 211, 153);  // Изумрудный неон
        case 3: return QColor(168, 85, 247);  // Фиолетовый квазар
        default: return QColor(56, 189, 248); // Голубой HyperOS
    }
}

bool Ton618GlWidget::isLightThemeActive() const {
    VisualParams vp = visuals();
    if (vp.highContrast) return false;
    if (vp.themeMode == 1) return true;
    if (vp.themeMode == 2 && QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
    }
    return false;
}

QColor Ton618GlWidget::resolveTextColor(const QColor &surfaceTint) const {
    VisualParams vp = visuals();
    bool light = isLightThemeActive();
    if (vp.highContrast) return QColor(255, 255, 255);
    if (vp.textColorMode == 1) return light ? QColor(15, 23, 42) : QColor(248, 250, 252);
    if (vp.textColorMode == 2) return light ? QColor(15, 23, 42) : QColor(226, 232, 240);
    if (light) return QColor(15, 23, 42);
    int lum = (surfaceTint.red() * 77 + surfaceTint.green() * 150 + surfaceTint.blue() * 29) >> 8;
    return (lum > 185) ? QColor(15, 23, 42) : QColor(248, 250, 252);
}

QImage Ton618GlWidget::buildCrystalFrostedImage(const QImage &src, int radius, int saturationPct) {
    if (src.isNull()) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    const int satScale = std::clamp(saturationPct, 80, 220);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp(lum + ((r - lum) * satScale) / 100 + 4, 0, 255);
            g = std::clamp(lum + ((g - lum) * satScale) / 100 + 4, 0, 255);
            b = std::clamp(lum + ((b - lum) * satScale) / 100 + 6, 0, 255);
            line[x] = qRgb(r, g, b);
        }
    }

    if (radius <= 0) return img;

    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);
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
                                           const QImage &prevMap,
                                           int speedPct,
                                           int strengthPct,
                                           QImage &outMap,
                                           QColor &outPrimary,
                                           QColor &outSecondary) {
    QImage cmap = srcSmall.scaled(32, 18, Qt::IgnoreAspectRatio, Qt::FastTransformation)
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

    float strengthFactor = std::clamp(strengthPct, 20, 150) / 100.0f;

    auto normalizeVibrant = [strengthFactor](double r, double g, double b) -> QColor {
        QColor c(std::clamp(int(r), 0, 255), std::clamp(int(g), 0, 255), std::clamp(int(b), 0, 255));
        float hf = 0.08f, sf = 0.7f, lf = 0.68f;
        c.getHslF(&hf, &sf, &lf);
        if (hf < 0.0f) hf = 0.08f;
        sf = std::clamp((sf * 1.35f + 0.20f) * strengthFactor, 0.25f, 0.96f);
        lf = std::clamp(lf * 1.12f + 0.28f, 0.62f, 0.80f);
        return QColor::fromHslF(hf, sf, lf);
    };

    outPrimary = normalizeVibrant(rAcc1 / wAcc1, gAcc1 / wAcc1, bAcc1 / wAcc1);
    outSecondary = normalizeVibrant(rAcc2 / wAcc2, gAcc2 / wAcc2, bAcc2 / wAcc2);

    bool hasPrev = (!prevMap.isNull() && prevMap.width() == w && prevMap.height() == h);
    int newWeight = std::clamp(speedPct, 5, 100);
    int oldWeight = 100 - newWeight;

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(cmap.scanLine(y));
        const QRgb *pLine = hasPrev ? reinterpret_cast<const QRgb *>(prevMap.constScanLine(y)) : nullptr;
        for (int x = 0; x < w; ++x) {
            QColor c(qRed(line[x]), qGreen(line[x]), qBlue(line[x]));
            float hf = 0.0f, sf = 0.0f, lf = 0.0f;
            c.getHslF(&hf, &sf, &lf);

            int nr = 0, ng = 0, nb = 0;
            if (lf < 0.12f || sf < 0.10f || hf < 0.0f) {
                double mix = double(x) / std::max(1, w - 1);
                int mr = int(outPrimary.red() * (1.0 - mix) + outSecondary.red() * mix);
                int mg = int(outPrimary.green() * (1.0 - mix) + outSecondary.green() * mix);
                int mb = int(outPrimary.blue() * (1.0 - mix) + outSecondary.blue() * mix);
                nr = (c.red() + mr * 2) / 3;
                ng = (c.green() + mg * 2) / 3;
                nb = (c.blue() + mb * 2) / 3;
            } else {
                sf = std::clamp((sf * 1.32f + 0.18f) * strengthFactor, 0.20f, 0.95f);
                lf = std::clamp(0.56f + lf * 0.30f, 0.60f, 0.82f);
                QColor vc = QColor::fromHslF(hf, sf, lf);
                nr = vc.red(); ng = vc.green(); nb = vc.blue();
            }

            if (pLine) {
                nr = (qRed(pLine[x]) * oldWeight + nr * newWeight) / 100;
                ng = (qGreen(pLine[x]) * oldWeight + ng * newWeight) / 100;
                nb = (qBlue(pLine[x]) * oldWeight + nb * newWeight) / 100;
            }
            line[x] = qRgb(nr, ng, nb);
        }
    }
    outMap = cmap;
}

QColor Ton618GlWidget::sampleAdaptiveColor(const QWidget *caller, const QPointF &localPos) const {
    VisualParams vp = visuals();
    if (vp.borderColorSource == 2) {
        return (vp.themeMode == 1) ? QColor(100, 116, 139) : QColor(226, 232, 240);
    }
    if (!m_chameleonEnabled.load() || vp.borderColorSource == 1) {
        return fixedAccentColor();
    }
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
    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    connect(m_resizeDebounce, &QTimer::timeout, this, [this]() {
        if (!m_animEnabled || m_videoPath.isEmpty()) {
            updateStaticCaches();
            update();
        }
    });

    loadBestFallbackImage();
    updateStaticCaches();

    if (auto *guiApp = qobject_cast<QGuiApplication *>(QGuiApplication::instance())) {
        connect(guiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            m_windowFocused = (state == Qt::ApplicationActive);
            updatePauseState();
        });
        connect(guiApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow *focusWin) {
            m_windowFocused = (focusWin != nullptr);
            updatePauseState();
        });
    }
}

Ton618GlWidget::~Ton618GlWidget() {
    stopDecoderThread();
}

void Ton618GlWidget::updatePauseState() {
    bool min = window() && window()->isMinimized();
    bool focusPaused = m_pauseOnFocusLoss.load() && !m_windowFocused;
    bool newPaused = (min || focusPaused || !m_animEnabled || m_inGamePaused || !isVisible());
    if (m_paused.exchange(newPaused) != newPaused) {
        m_pauseCv.notify_all();
    }
}

void Ton618GlWidget::applyVisualSettings(const AppSettings &s) {
    int effectiveBlur = s.glassBlurStrength;
    if (s.disableBlurLowGpu || s.effectsQuality == 0) effectiveBlur = 0;
    else if (s.effectsQuality == 1) effectiveBlur = std::min(effectiveBlur, 3);

    bool needStaticRebuild = false;
    bool prevAnim = m_animEnabled;
    bool wantVideo = s.animBg && (s.backdropType == 0 || s.backdropType == 3);

    {
        std::lock_guard<std::mutex> lk(m_visMutex);
        if (m_vis.backdropType != s.backdropType || m_vis.glassSaturationPct != s.glassSaturationPct) {
            needStaticRebuild = true;
        }
        m_vis.blurStrength = effectiveBlur;
        m_vis.glassOpacityPct = s.reduceTransparency ? std::max(45, s.glassOpacityPct) : s.glassOpacityPct;
        m_vis.borderThicknessPx = s.highContrast ? std::max(16, s.borderThicknessPx) : s.borderThicknessPx;
        m_vis.borderBrightnessPct = s.highContrast ? std::max(145, s.borderBrightnessPct) : s.borderBrightnessPct;
        m_vis.glassSaturationPct = s.glassSaturationPct;
        m_vis.specularStrengthPct = (s.effectsQuality == 0) ? 0 : s.specularStrengthPct;
        m_vis.shadowStrengthPct = (s.effectsQuality == 0) ? 0 : s.shadowStrengthPct;
        m_vis.cornerRadiusMode = s.cornerRadiusMode;
        m_vis.glassStyleMode = s.glassStyleMode;

        m_vis.themeMode = s.themeMode;
        m_vis.accentPreset = s.accentPreset;
        QColor hexCol(s.customAccentHex.trimmed());
        m_vis.customAccent = hexCol.isValid() ? hexCol : QColor(56, 189, 248);
        m_vis.chameleonEnabled = s.chameleonEnabled && !s.highContrast;
        m_vis.chameleonSpeedPct = s.chameleonSpeedPct;
        m_vis.chameleonStrengthPct = s.chameleonStrengthPct;
        m_vis.borderColorSource = s.borderColorSource;
        m_vis.textColorMode = s.textColorMode;

        m_vis.backdropType = s.backdropType;
        m_vis.backdropDimPct = s.backdropDimPct;
        m_vis.videoFpsLimit = s.videoFpsLimit;
        m_vis.pauseOnFocusLoss = s.pauseOnFocusLoss;

        m_vis.animationsEnabled = s.animationsEnabled && !s.reducedMotion;
        m_vis.animSpeedPct = s.animSpeedPct;
        m_vis.animStyle = (!s.animationsEnabled || s.reducedMotion) ? 2 : s.animStyle;
        m_vis.cursorGlowFollow = s.cursorGlowFollow && !s.reducedMotion && (s.effectsQuality > 0);
        m_vis.reducedMotion = s.reducedMotion || !s.animationsEnabled || (s.animStyle == 2);

        m_vis.highContrast = s.highContrast;
        m_vis.reduceTransparency = s.reduceTransparency;
        m_vis.effectsQuality = s.effectsQuality;
        m_vis.disableBlurLowGpu = s.disableBlurLowGpu;
    }

    if (m_blurStrength.exchange(effectiveBlur) != effectiveBlur) needStaticRebuild = true;
    m_saturationPct = s.glassSaturationPct;
    m_glassOpacityPct = s.reduceTransparency ? std::max(45, s.glassOpacityPct) : s.glassOpacityPct;
    m_borderBrightnessPct = s.borderBrightnessPct;
    m_chameleonEnabled = s.chameleonEnabled && !s.highContrast;
    m_chameleonSpeed = s.chameleonSpeedPct;
    m_chameleonStrength = s.chameleonStrengthPct;
    m_reducedMotion = s.reducedMotion || !s.animationsEnabled || (s.animStyle == 2);
    m_videoFps = std::clamp(s.videoFpsLimit, 15, 60);
    m_accentPreset = s.accentPreset;
    m_pauseOnFocusLoss = s.pauseOnFocusLoss;

    if (s.backdropType == 3 && !s.customBackdropPath.trimmed().isEmpty() && QFileInfo::exists(s.customBackdropPath.trimmed())) {
        QString cPath = s.customBackdropPath.trimmed();
        if (m_customFilePath != cPath) {
            m_customFilePath = cPath;
            loadCustomVideoFile(cPath);
            return;
        }
    } else if (s.backdropType == 0 && !m_defaultVideoPath.isEmpty()) {
        bool restoredDefault = false;
        {
            std::lock_guard<std::mutex> lk(m_pathMutex);
            if (m_videoPath != m_defaultVideoPath) {
                m_videoPath = m_defaultVideoPath;
                m_customFilePath.clear();
                restoredDefault = true;
            }
        }
        if (restoredDefault) {
            loadBestFallbackImage();
            updateStaticCaches();
            if (wantVideo) startDecoderThread();
        }
    } else if (s.backdropType == 1) {
        m_customFilePath.clear();
        loadBestFallbackImage();
    }

    if (prevAnim != wantVideo) {
        setAnimationEnabled(wantVideo);
    } else if (needStaticRebuild) {
        updateStaticCaches();
    }
    updatePauseState();
    if (window()) window()->update();
}

void Ton618GlWidget::configureVisuals(int blurStrength,
                                      int glassOpacityPct,
                                      int borderBrightnessPct,
                                      bool chameleon,
                                      bool reducedMotion,
                                      int fpsLimit,
                                      int accentPreset) {
    bool blurChanged = (m_blurStrength.exchange(std::clamp(blurStrength, 0, 8)) != blurStrength);
    m_glassOpacityPct = std::clamp(glassOpacityPct, 2, 60);
    m_borderBrightnessPct = std::clamp(borderBrightnessPct, 20, 180);
    m_chameleonEnabled = chameleon;
    m_reducedMotion = reducedMotion;
    m_videoFps = std::clamp(fpsLimit, 15, 60);
    m_accentPreset = std::clamp(accentPreset, 0, 4);
    if (blurChanged) updateStaticCaches();
    if (window()) window()->update();
}

bool Ton618GlWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == window()) {
        if (event->type() == QEvent::WindowStateChange) {
            updatePauseState();
        } else if (event->type() == QEvent::WindowActivate) {
            m_windowFocused = true;
            updatePauseState();
        } else if (event->type() == QEvent::WindowDeactivate) {
            m_windowFocused = false;
            updatePauseState();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Ton618GlWidget::startDecoderThread() {
    stopDecoderThread();
    if (m_videoPath.isEmpty() || !m_animEnabled) return;
    m_stopWorker = false;
    m_worker = std::thread(&Ton618GlWidget::decoderLoop, this);
}

void Ton618GlWidget::stopDecoderThread() {
    m_stopWorker = true;
    m_pauseCv.notify_all();
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

    QString escapedPath = path;
    escapedPath.replace("'", "'\\''");
    QString cmd = QString("ffmpeg -loglevel quiet -re -threads 1 -stream_loop -1 -i '%1' "
                          "-vf scale=%2:%3:flags=fast_bilinear -r 24 -f rawvideo -pix_fmt rgb32 -")
                      .arg(escapedPath)
                      .arg(STREAM_W)
                      .arg(STREAM_H);

    FILE *pipe = popen(cmd.toUtf8().constData(), "r");
    if (!pipe) return;

    std::vector<uchar> buf(FRAME_BYTES);
    QImage prevColorMap;
    QColor cachedAmb1 = m_ambientPrimary;
    QColor cachedAmb2 = m_ambientSecondary;
    uint32_t frameCounter = 0;
    auto nextTick = std::chrono::steady_clock::now();
    QPointer<Ton618GlWidget> safeThis(this);

    while (!m_stopWorker.load(std::memory_order_relaxed)) {
        if (m_paused.load(std::memory_order_relaxed)) {
            std::unique_lock<std::mutex> lk(m_pauseMutex);
            m_pauseCv.wait_for(lk, std::chrono::milliseconds(250), [this]() {
                return m_stopWorker.load() || !m_paused.load();
            });
            nextTick = std::chrono::steady_clock::now();
            continue;
        }

        size_t readBytes = 0;
        while (readBytes < FRAME_BYTES && !m_stopWorker.load(std::memory_order_relaxed)) {
            size_t n = std::fread(buf.data() + readBytes, 1, FRAME_BYTES - readBytes, pipe);
            if (n == 0) break;
            readBytes += n;
        }
        if (readBytes < FRAME_BYTES || m_stopWorker.load(std::memory_order_relaxed)) break;

        int tw = std::max(320, m_targetW.load(std::memory_order_relaxed));
        int th = std::max(240, m_targetH.load(std::memory_order_relaxed));
        int blurR = std::clamp(m_blurStrength.load(std::memory_order_relaxed), 0, 8);
        int satPct = std::clamp(m_saturationPct.load(std::memory_order_relaxed), 80, 220);

        if (!m_frameQueued.load(std::memory_order_acquire)) {
            QImage rawFrame(buf.data(), STREAM_W, STREAM_H, STREAM_W * 4, QImage::Format_RGB32);

            QImage sharpScaled = rawFrame.scaled(tw, th, Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);
            int offX = std::max(0, (sharpScaled.width() - tw) / 2);
            int offY = std::max(0, (sharpScaled.height() - th) / 2);
            QImage sharpFinal = sharpScaled.copy(offX, offY, tw, th);

            QImage small = rawFrame.scaled(160, 90, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            QImage blurredSmall = buildCrystalFrostedImage(small, blurR, satPct);
            QImage frostedFinal = blurredSmall.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::FastTransformation);

            if ((frameCounter++ % 4 == 0) || prevColorMap.isNull()) {
                int cSpeed = std::clamp(m_chameleonSpeed.load(std::memory_order_relaxed), 5, 100);
                int cStrength = std::clamp(m_chameleonStrength.load(std::memory_order_relaxed), 20, 150);
                QImage cmap;
                buildAdaptiveColorMap(blurredSmall, prevColorMap, cSpeed, cStrength, cmap, cachedAmb1, cachedAmb2);
                prevColorMap = std::move(cmap);
            }
            QImage cmapCopy = prevColorMap;
            QColor amb1 = cachedAmb1;
            QColor amb2 = cachedAmb2;

            m_frameQueued.store(true, std::memory_order_release);
            QMetaObject::invokeMethod(this,
                [safeThis,
                 sImg = std::move(sharpFinal),
                 fImg = std::move(frostedFinal),
                 cImg = std::move(cmapCopy),
                 amb1, amb2]() mutable {
                    if (!safeThis) return;
                    safeThis->m_scaledSharp = QPixmap::fromImage(std::move(sImg));
                    safeThis->m_scaledFrosted = QPixmap::fromImage(std::move(fImg));
                    safeThis->m_colorMap = std::move(cImg);
                    safeThis->m_ambientPrimary = amb1;
                    safeThis->m_ambientSecondary = amb2;
                    safeThis->m_frameQueued.store(false, std::memory_order_release);
                    if (safeThis->isVisible() && (!safeThis->window() || !safeThis->window()->isMinimized())) {
                        safeThis->update();
                    }
                }, Qt::QueuedConnection);
        }

        int fps = std::clamp(m_videoFps.load(std::memory_order_relaxed), 15, 30);
        nextTick += std::chrono::microseconds(1000000 / fps);
        auto now = std::chrono::steady_clock::now();
        if (nextTick > now) {
            std::unique_lock<std::mutex> lk(m_pauseMutex);
            m_pauseCv.wait_until(lk, nextTick, [this]() { return m_stopWorker.load(); });
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

    const QString destDir = QDir::homePath() + "/.local/share/mqblox/videos";
    QDir().mkpath(destDir);
    QString dest = destDir + "/custom_loop.mp4";
    auto *conv = new QProcess(this);
    connect(conv, &QProcess::finished, this, [this, conv, dest](int code, QProcess::ExitStatus) {
        conv->deleteLater();
        if (code == 0 && QFileInfo(dest).size() > 10000) {
            {
                std::lock_guard<std::mutex> lk(m_pathMutex);
                m_videoPath = dest;
            }
            if (m_animEnabled) startDecoderThread();
        }
    });
    conv->start("ffmpeg", QStringList{
        "-loglevel", "error", "-y",
        "-i", filePath,
        "-vf", "scale=854:480:flags=lanczos,fps=30",
        "-c:v", "libx264", "-preset", "ultrafast", "-crf", "16", "-g", "15",
        "-pix_fmt", "yuv420p", "-an", dest
    });
}

void Ton618GlWidget::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    if (window()) {
        window()->removeEventFilter(this);
        window()->installEventFilter(this);
    }
    updatePauseState();
}

void Ton618GlWidget::hideEvent(QHideEvent *e) {
    QWidget::hideEvent(e);
    m_paused = true;
}

void Ton618GlWidget::setAnimationEnabled(bool enabled) {
    if (m_animEnabled == enabled) return;
    m_animEnabled = enabled;
    updatePauseState();
    if (!m_animEnabled) {
        stopDecoderThread();
        updateStaticCaches();
    } else if (!m_videoPath.isEmpty()) {
        startDecoderThread();
    }
    update();
}

void Ton618GlWidget::setInGamePaused(bool paused) {
    m_inGamePaused = paused;
    updatePauseState();
}

void Ton618GlWidget::setWebTabActive(bool) {}

void Ton618GlWidget::loadBestFallbackImage() {
    const QString homeBase = QDir::homePath() + "/.local/share/mqblox";
    const QString appBase = QCoreApplication::applicationDirPath() + "/../share/mqblox";
    const QStringList baseDirs = {
        homeBase,
        appBase,
        "/usr/local/share/mqblox",
        "/usr/share/mqblox"
    };
    for (const QString &base : baseDirs) {
        for (const char *imgName : {
                 "pexels_39811262_uhd.png",
                 "ton618_4k_master_v22.png",
                 "ton618_4k_master_v21.png"}) {
            const QString candidate = base + "/" + imgName;
            if (QFileInfo(candidate).size() > 50000 && m_fallbackImg.load(candidate)) {
                return;
            }
        }
    }
    m_fallbackImg = QImage(STREAM_W, STREAM_H, QImage::Format_RGB32);
    m_fallbackImg.fill(QColor(4, 6, 14));
}

void Ton618GlWidget::updateStaticCaches() {
    if (width() <= 10 || height() <= 10) return;

    VisualParams vp = visuals();
    QImage baseImg = m_fallbackImg;

    if (vp.backdropType == 2) {
        // Космический градиент (0% CPU/GPU)
        QImage gradImg(320, 180, QImage::Format_RGB32);
        QPainter gp(&gradImg);
        QLinearGradient lg(0, 0, 320, 180);
        if (isLightThemeActive()) {
            lg.setColorAt(0.0, QColor(224, 242, 254));
            lg.setColorAt(0.5, QColor(241, 245, 249));
            lg.setColorAt(1.0, QColor(226, 232, 240));
        } else {
            QColor acc = fixedAccentColor();
            lg.setColorAt(0.0, QColor(6, 10, 24));
            lg.setColorAt(0.55, QColor(acc.red() / 5, acc.green() / 5, acc.blue() / 4));
            lg.setColorAt(1.0, QColor(4, 6, 14));
        }
        gp.fillRect(gradImg.rect(), lg);
        gp.end();
        baseImg = gradImg;
    } else if (baseImg.isNull()) {
        loadBestFallbackImage();
        baseImg = m_fallbackImg;
    }

    QImage sharpScaled = baseImg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int offX = std::max(0, (sharpScaled.width() - width()) / 2);
    int offY = std::max(0, (sharpScaled.height() - height()) / 2);
    QImage cropped = sharpScaled.copy(offX, offY, width(), height());
    m_scaledSharp = QPixmap::fromImage(cropped);

    QImage small = cropped.scaled(240, 135, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage blurredSmall = buildCrystalFrostedImage(small, std::clamp(m_blurStrength.load(), 0, 8), m_saturationPct.load());
    m_scaledFrosted = QPixmap::fromImage(blurredSmall.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    QImage prev;
    buildAdaptiveColorMap(blurredSmall, prev, m_chameleonSpeed.load(), m_chameleonStrength.load(),
                          m_colorMap, m_ambientPrimary, m_ambientSecondary);
}

void Ton618GlWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    m_targetW = std::max(320, width());
    m_targetH = std::max(240, height());
    if (m_scaledSharp.isNull()) {
        updateStaticCaches();
    } else if (!m_animEnabled || m_videoPath.isEmpty()) {
        m_resizeDebounce->start(40);
    }
}

void Ton618GlWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int w = width(), h = height();
    VisualParams vp = visuals();

    double winRadius = (vp.cornerRadiusMode == 0) ? 14.0 : ((vp.cornerRadiusMode == 2) ? 26.0 : 22.0);
    QPainterPath clip;
    clip.addRoundedRect(1.0, 1.0, w - 2.0, h - 2.0, winRadius, winRadius);
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

    // Затемнение заднего фона (0..85%) или осветление для светлой темы
    if (isLightThemeActive()) {
        p.fillRect(rect(), QColor(248, 250, 252, 165));
    } else if (vp.backdropDimPct > 0) {
        int dimAlpha = std::clamp(vp.backdropDimPct * 255 / 100, 0, 225);
        p.fillRect(rect(), QColor(2, 4, 10, dimAlpha));
    }

    QLinearGradient winRim(0, 0, w, h);
    QColor c1 = m_chameleonEnabled.load() ? m_ambientPrimary : fixedAccentColor();
    QColor c2 = m_chameleonEnabled.load() ? m_ambientSecondary : fixedAccentColor().darker(125);
    c1.setAlpha(135);
    c2.setAlpha(95);
    winRim.setColorAt(0.0, QColor(255, 255, 255, 145));
    winRim.setColorAt(0.5, c1);
    winRim.setColorAt(1.0, c2);
    p.setPen(QPen(QBrush(winRim), std::clamp(vp.borderThicknessPx / 10.0, 0.5, 2.5)));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(1, 1, w - 2, h - 2, winRadius, winRadius);
}

} // namespace mqblox
