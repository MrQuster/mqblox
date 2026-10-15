#pragma once
#include <QImage>
#include <QPixmap>
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

    const QPixmap &sharpBackdrop() const { return m_scaledSharp; }
    const QPixmap &frostedBackdrop() const { return m_scaledFrosted; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    void startDecoderThread();
    void stopDecoderThread();
    void decoderLoop();
    void updateStaticCaches();
    void loadBestFallbackImage();
    static QImage buildCrystalFrostedImage(const QImage &src, int radius);

    std::thread m_worker;
    std::atomic<bool> m_stopWorker{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_frameQueued{false};
    std::atomic<int> m_targetW{1100};
    std::atomic<int> m_targetH{660};

    std::mutex m_pathMutex;
    QString m_videoPath;

    QImage m_fallbackImg;
    QPixmap m_scaledSharp;
    QPixmap m_scaledFrosted;
    bool m_animEnabled = true;
    bool m_inGamePaused = false;

    static constexpr int STREAM_W = 854;
    static constexpr int STREAM_H = 480;
    static constexpr int FRAME_BYTES = STREAM_W * STREAM_H * 4;
};

} // namespace mqblox
