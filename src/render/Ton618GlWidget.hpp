#pragma once
#include <QByteArray>
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QWidget>

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
    void onStreamReadyRead();
    void updateFrameAndFrostedMap(const QImage &srcImg);
    void loadBestFallbackImage();
    void syncPlaybackState();
    static QImage applyHyperOsVibrantBlur(const QImage &src, int radius);

    QProcess *m_ffmpegProc = nullptr;
    QString m_videoPath;
    QByteArray m_rawBuf;
    QImage m_currentFrameImg;
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
