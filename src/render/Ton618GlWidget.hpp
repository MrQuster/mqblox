#pragma once
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

class Ton618GlWidget : public QWidget {
    Q_OBJECT
public:
    explicit Ton618GlWidget(QWidget *parent = nullptr);
    ~Ton618GlWidget() override;

    void setAnimationEnabled(bool enabled);
    void setInGamePaused(bool paused);
    void setWebTabActive(bool webActive);
    void loadCustomVideoFile(const QString &filePath);

    void configureVisuals(int blurStrength,
                          int glassOpacityPct,
                          int borderBrightnessPct,
                          bool chameleon,
                          bool reducedMotion,
                          int fpsLimit,
                          int accentPreset);

    int glassOpacityPct() const { return m_glassOpacityPct.load(); }
    int borderBrightnessPct() const { return m_borderBrightnessPct.load(); }
    bool isChameleonEnabled() const { return m_chameleonEnabled.load(); }
    bool isReducedMotion() const { return m_reducedMotion.load(); }
    QColor fixedAccentColor() const;

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
    static QImage buildCrystalFrostedImage(const QImage &src, int radius);
    static void buildAdaptiveColorMap(const QImage &srcSmall,
                                      const QImage &prevMap,
                                      QImage &outMap,
                                      QColor &outPrimary,
                                      QColor &outSecondary);

    std::thread m_worker;
    std::atomic<bool> m_stopWorker{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_frameQueued{false};
    std::atomic<int> m_targetW{1100};
    std::atomic<int> m_targetH{660};

    std::atomic<int> m_blurStrength{4};
    std::atomic<int> m_glassOpacityPct{10};
    std::atomic<int> m_borderBrightnessPct{100};
    std::atomic<bool> m_chameleonEnabled{true};
    std::atomic<bool> m_reducedMotion{false};
    std::atomic<int> m_videoFps{30};
    std::atomic<int> m_accentPreset{0};

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

    static constexpr int STREAM_W = 854;
    static constexpr int STREAM_H = 480;
    static constexpr int FRAME_BYTES = STREAM_W * STREAM_H * 4;
};

} // namespace mqblox
