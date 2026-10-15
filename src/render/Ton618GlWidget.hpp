#pragma once
#include <QImage>
#include <QPixmap>
#include <QWidget>

namespace mqblox {

class Ton618GlWidget : public QWidget {
    Q_OBJECT
public:
    explicit Ton618GlWidget(QWidget *parent = nullptr);
    ~Ton618GlWidget() override = default;

    void setAnimationEnabled(bool enabled);
    void setInGamePaused(bool paused);
    void setWebTabActive(bool webActive);
    void loadCustomVideoFile(const QString &filePath);

    const QPixmap &sharpBackdrop() const { return m_scaledSharp; }
    const QPixmap &frostedBackdrop() const { return m_scaledFrosted; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void loadBestBackdropImage();
    void rebuildWindowCaches();
    static QImage applySmoothGaussianBlur(const QImage &src, int radius);

    QImage m_masterImg;
    QPixmap m_scaledSharp;
    QPixmap m_scaledFrosted;
};

} // namespace mqblox
