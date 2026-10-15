#include "render/Ton618GlWidget.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <algorithm>

namespace mqblox {

QImage Ton618GlWidget::applySmoothGaussianBlur(const QImage &src, int radius) {
    if (src.isNull() || radius <= 1) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp((lum + ((r - lum) * 145) / 100) * 65 / 100 + 10, 0, 255);
            g = std::clamp((lum + ((g - lum) * 140) / 100) * 65 / 100 + 13, 0, 255);
            b = std::clamp((lum + ((b - lum) * 150) / 100) * 68 / 100 + 22, 0, 255);
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
    loadBestBackdropImage();
}

void Ton618GlWidget::setAnimationEnabled(bool) {}
void Ton618GlWidget::setInGamePaused(bool) {}
void Ton618GlWidget::setWebTabActive(bool) {}

void Ton618GlWidget::loadCustomVideoFile(const QString &filePath) {
    if (!QFile::exists(filePath)) return;
    QString frameOut = QDir::homePath() + "/.local/share/mqblox/pexels_39811262_uhd.png";
    if (filePath.endsWith(".png", Qt::CaseInsensitive) || filePath.endsWith(".jpg", Qt::CaseInsensitive)) {
        m_masterImg.load(filePath);
    } else {
        QProcess::execute("ffmpeg", {"-loglevel", "error", "-y", "-ss", "00:00:03.00", "-i", filePath, "-frames:v", "1", frameOut});
        m_masterImg.load(frameOut);
    }
    rebuildWindowCaches();
    update();
}

void Ton618GlWidget::loadBestBackdropImage() {
    QString base = QDir::homePath() + "/.local/share/mqblox";
    for (const QString &candidate : {
             base + "/pexels_39811262_uhd.png",
             base + "/ton618_4k_master_v22.png",
             base + "/ton618_4k_master_v21.png"}) {
        if (QFileInfo(candidate).size() > 50000 && m_masterImg.load(candidate)) {
            return;
        }
    }
    m_masterImg = QImage(1280, 720, QImage::Format_ARGB32_Premultiplied);
    m_masterImg.fill(QColor(4, 6, 14));
}

void Ton618GlWidget::rebuildWindowCaches() {
    if (width() <= 10 || height() <= 10 || m_masterImg.isNull()) return;

    QImage scaled = m_masterImg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int offX = std::max(0, (scaled.width() - width()) / 2);
    int offY = std::max(0, (scaled.height() - height()) / 2);
    QImage cropped = scaled.copy(offX, offY, width(), height());

    QPainter darken(&cropped);
    darken.fillRect(cropped.rect(), QColor(2, 4, 10, 45));
    darken.end();
    m_scaledSharp = QPixmap::fromImage(cropped);

    QImage medium = cropped.scaled(std::max(1, width() / 3), std::max(1, height() / 3),
                                   Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage blurred = applySmoothGaussianBlur(medium, 12);
    m_scaledFrosted = QPixmap::fromImage(
        blurred.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void Ton618GlWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    rebuildWindowCaches();
}

void Ton618GlWidget::paintEvent(QPaintEvent *) {
    if (m_scaledSharp.isNull() || m_scaledSharp.size() != size()) {
        rebuildWindowCaches();
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int w = width(), h = height();

    QPainterPath clip;
    clip.addRoundedRect(1.0, 1.0, w - 2.0, h - 2.0, 18.0, 18.0);
    p.setClipPath(clip);
    p.drawPixmap(0, 0, m_scaledSharp);

    p.setPen(QPen(QColor(255, 255, 255, 115), 1.4));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(1, 1, w - 2, h - 2, 18, 18);
}

} // namespace mqblox
