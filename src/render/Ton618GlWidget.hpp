#pragma once
#include "core/Types.hpp"
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QTimer>
#include <QWidget>
#include <atomic>
#include <mutex>
#include <thread>

namespace mqblox {

struct VisualParams {
    int blurStrength = 4;
    int glassOpacityPct = 10;
    int borderThicknessPx = 10;
    int borderBrightnessPct = 100;
    int glassSaturationPct = 160;
    int specularStrengthPct = 100;
    int shadowStrengthPct = 100;
    int cornerRadiusMode = 1;
    int glassStyleMode = 0;

    int themeMode = 0;
    int accentPreset = 0;
    QColor customAccent{56, 189, 248};
    bool chameleonEnabled = true;
    int chameleonSpeedPct = 20;
    int chameleonStrengthPct = 100;
    int borderColorSource = 0;
    int textColorMode = 0;

    int backdropType = 0;
    int backdropDimPct = 0;
    int videoFpsLimit = 30;
    bool pauseOnFocusLoss = false;

    bool animationsEnabled = true;
    int animSpeedPct = 100;
    int animStyle = 1;
    bool cursorGlowFollow = true;
    bool reducedMotion = false;

    bool highContrast = false;
    bool reduceTransparency = false;
    int effectsQuality = 2;
    bool disableBlurLowGpu = false;
};

class Ton618GlWidget : public QWidget {
    Q_OBJECT
public:
    explicit Ton618GlWidget(QWidget *parent = nullptr);
    ~Ton618GlWidget() override;

    void setAnimationEnabled(bool enabled);
    void setInGamePaused(bool paused);
    void setWebTabActive(bool webActive);
    void loadCustomVideoFile(const QString &filePath);

    void applyVisualSettings(const AppSettings &s);
    void configureVisuals(int blurStrength,
                          int glassOpacityPct,
                          int borderBrightnessPct,
                          bool chameleon,
                          bool reducedMotion,
                          int fpsLimit,
                          int accentPreset);

    VisualParams visuals() const {
        std::lock_guard<std::mutex> lk(m_visMutex);
        return m_vis;
    }

    int glassOpacityPct() const { return m_glassOpacityPct.load(); }
    int borderBrightnessPct() const { return m_borderBrightnessPct.load(); }
    bool isChameleonEnabled() const { return m_chameleonEnabled.load(); }
    bool isReducedMotion() const { return m_reducedMotion.load(); }
    QColor fixedAccentColor() const;
    QColor resolveTextColor(const QColor &surfaceTint) const;

    const QPixmap &sharpBackdrop() const { return m_scaledSharp; }
    const QPixmap &frostedBackdrop() const { return m_scaledFrosted; }

    QColor sampleAdaptiveColor(const QWidget *caller, const QPointF &localPos) const;
    QColor ambientPrimary() const { return m_ambientPrimary; }
    QColor ambientSecondary() const { return m_ambientSecondary; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void startDecoderThread();
    void stopDecoderThread();
    void decoderLoop();
    void updateStaticCaches();
    void loadBestFallbackImage();
    void updatePauseState();
    static QImage buildCrystalFrostedImage(const QImage &src, int radius, int saturationPct);
    static void buildAdaptiveColorMap(const QImage &srcSmall,
                                      const QImage &prevMap,
                                      int speedPct,
                                      int strengthPct,
                                      QImage &outMap,
                                      QColor &outPrimary,
                                      QColor &outSecondary);

    std::thread m_worker;
    std::atomic<bool> m_stopWorker{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_frameQueued{false};
    std::atomic<int> m_targetW{1100};
    std::atomic<int> m_targetH{660};

    mutable std::mutex m_visMutex;
    VisualParams m_vis;

    std::atomic<int> m_blurStrength{4};
    std::atomic<int> m_saturationPct{160};
    std::atomic<int> m_glassOpacityPct{10};
    std::atomic<int> m_borderBrightnessPct{100};
    std::atomic<bool> m_chameleonEnabled{true};
    std::atomic<int> m_chameleonSpeed{20};
    std::atomic<int> m_chameleonStrength{100};
    std::atomic<bool> m_reducedMotion{false};
    std::atomic<int> m_videoFps{30};
    std::atomic<int> m_accentPreset{0};
    std::atomic<bool> m_pauseOnFocusLoss{false};

    std::mutex m_pathMutex;
    QString m_videoPath;

    QTimer *m_resizeDebounce = nullptr;
    QImage m_fallbackImg;
    QImage m_colorMap;
    QColor m_ambientPrimary{251, 191, 36};
    QColor m_ambientSecondary{56, 189, 248};
    QPixmap m_scaledSharp;
    QPixmap m_scaledFrosted;
    bool m_animEnabled = true;
    bool m_inGamePaused = false;
    bool m_windowFocused = true;

    static constexpr int STREAM_W = 854;
    static constexpr int STREAM_H = 480;
    static constexpr int FRAME_BYTES = STREAM_W * STREAM_H * 4;
};

} // namespace mqblox
