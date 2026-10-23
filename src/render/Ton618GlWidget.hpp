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

namespace mqblox {

struct VisualParams {
    int blurStrength = 0;
    int glassOpacityPct = 100;
    int borderThicknessPx = 10;
    int borderBrightnessPct = 100;
    int glassSaturationPct = 100;
    int specularStrengthPct = 0;
    int shadowStrengthPct = 60;
    int cornerRadiusMode = 1;
    int glassStyleMode = 0;

    int themeMode = 0;
    int accentPreset = 0;
    QColor customAccent{56, 189, 248};
    bool chameleonEnabled = false;
    int chameleonSpeedPct = 20;
    int chameleonStrengthPct = 100;
    int borderColorSource = 0;
    int textColorMode = 0;

    int backdropType = 0;
    int backdropDimPct = 0;
    int videoFpsLimit = 60;
    bool pauseOnFocusLoss = true;

    bool animationsEnabled = true;
    int animSpeedPct = 100;
    int animStyle = 1;
    bool cursorGlowFollow = false;
    bool reducedMotion = false;

    bool highContrast = false;
    bool reduceTransparency = true;
    int effectsQuality = 1;
    bool disableBlurLowGpu = true;
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

    int glassOpacityPct() const { return 100; }
    int borderBrightnessPct() const { return m_borderBrightnessPct.load(); }
    bool isChameleonEnabled() const { return false; }
    bool isReducedMotion() const { return m_reducedMotion.load(); }
    QColor fixedAccentColor() const;
    bool isLightThemeActive() const;
    QColor resolveTextColor(const QColor &surfaceTint) const;

    const QPixmap &sharpBackdrop() const { return m_dummyBackdrop; }
    const QPixmap &frostedBackdrop() const { return m_dummyBackdrop; }

    QColor sampleAdaptiveColor(const QWidget *caller, const QPointF &localPos) const;
    QColor ambientPrimary() const { return fixedAccentColor(); }
    QColor ambientSecondary() const { return QColor(56, 189, 248); }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    mutable std::mutex m_visMutex;
    VisualParams m_vis;

    std::atomic<int> m_borderBrightnessPct{100};
    std::atomic<bool> m_reducedMotion{false};
    std::atomic<int> m_accentPreset{0};
    bool m_animEnabled = true;
    bool m_inGamePaused = false;
    QPixmap m_dummyBackdrop;
};

} // namespace mqblox
