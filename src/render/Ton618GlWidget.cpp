#include "render/Ton618GlWidget.hpp"
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QStyleHints>
#include <QtGlobal>
#include <algorithm>

namespace mqblox {

Ton618GlWidget::Ton618GlWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

Ton618GlWidget::~Ton618GlWidget() = default;

void Ton618GlWidget::setAnimationEnabled(bool enabled) {
    m_animEnabled = enabled;
    update();
}

void Ton618GlWidget::setInGamePaused(bool paused) {
    m_inGamePaused = paused;
    update();
}

void Ton618GlWidget::setWebTabActive(bool) {}

void Ton618GlWidget::loadCustomVideoFile(const QString &) {}

QColor Ton618GlWidget::fixedAccentColor() const {
    int preset = m_accentPreset.load();
    if (preset == 4) {
        std::lock_guard<std::mutex> lk(m_visMutex);
        if (m_vis.customAccent.isValid()) return m_vis.customAccent;
    }
    switch (preset) {
        case 1: return QColor(245, 158, 11);  // Amber Gold
        case 2: return QColor(16, 185, 129);  // Emerald
        case 3: return QColor(168, 85, 247);  // Quasar Violet
        default: return QColor(56, 189, 248); // Electric Sky
    }
}

bool Ton618GlWidget::isLightThemeActive() const {
    VisualParams vp = visuals();
    if (vp.highContrast) return false;
    if (vp.themeMode == 1) return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (vp.themeMode == 2 && QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
    }
#endif
    return false;
}

QColor Ton618GlWidget::resolveTextColor(const QColor &) const {
    if (isLightThemeActive()) return QColor(15, 23, 42);
    return QColor(248, 250, 252);
}

QColor Ton618GlWidget::sampleAdaptiveColor(const QWidget *, const QPointF &) const {
    return fixedAccentColor();
}

void Ton618GlWidget::applyVisualSettings(const AppSettings &s) {
    {
        std::lock_guard<std::mutex> lk(m_visMutex);
        m_vis.themeMode = s.themeMode;
        m_vis.accentPreset = s.accentPreset;
        QColor hexCol(s.customAccentHex.trimmed());
        m_vis.customAccent = hexCol.isValid() ? hexCol : QColor(56, 189, 248);
        m_vis.animationsEnabled = s.animationsEnabled;
        m_vis.animSpeedPct = s.animSpeedPct;
        m_vis.animStyle = s.animStyle;
        m_vis.reducedMotion = s.reducedMotion || !s.animationsEnabled;
        m_vis.highContrast = s.highContrast;
        m_vis.cornerRadiusMode = s.cornerRadiusMode;
    }
    m_accentPreset = s.accentPreset;
    m_reducedMotion = s.reducedMotion || !s.animationsEnabled;
    update();
}

void Ton618GlWidget::configureVisuals(int, int, int borderBrightnessPct, bool, bool reducedMotion, int, int accentPreset) {
    m_borderBrightnessPct = std::clamp(borderBrightnessPct, 20, 180);
    m_reducedMotion = reducedMotion;
    m_accentPreset = std::clamp(accentPreset, 0, 4);
    update();
}

bool Ton618GlWidget::eventFilter(QObject *watched, QEvent *event) {
    return QWidget::eventFilter(watched, event);
}

void Ton618GlWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    update();
}

void Ton618GlWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    if (w <= 2 || h <= 2) return;

    VisualParams vp = visuals();
    const bool light = isLightThemeActive();
    const double winRadius = (vp.cornerRadiusMode == 0) ? 12.0 : ((vp.cornerRadiusMode == 2) ? 22.0 : 16.0);

    QRectF fullRect(0.5, 0.5, w - 1.0, h - 1.0);
    QPainterPath path;
    path.addRoundedRect(fullRect, winRadius, winRadius);
    p.setClipPath(path);

    if (vp.highContrast) {
        p.fillRect(rect(), QColor(5, 7, 13));
    } else if (light) {
        // Clean Minimalist Light Mode
        QLinearGradient lg(0, 0, 0, h);
        lg.setColorAt(0.0, QColor(248, 250, 252));
        lg.setColorAt(1.0, QColor(241, 245, 249));
        p.fillRect(rect(), lg);
    } else {
        // Clean Minimalist Deep Dark Mode (Vercel / Raycast / Linear dark aesthetic)
        QColor baseBg(10, 13, 22);
        QColor subtleTint = fixedAccentColor();

        p.fillRect(rect(), baseBg);

        // Subtle ambient radial glow at top
        QRadialGradient ambient(QPointF(w * 0.25, 0), w * 0.75);
        ambient.setColorAt(0.0, QColor(subtleTint.red(), subtleTint.green(), subtleTint.blue(), 18));
        ambient.setColorAt(0.6, QColor(subtleTint.red(), subtleTint.green(), subtleTint.blue(), 4));
        ambient.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), ambient);
    }

    // Crisp 1px window border
    p.setClipping(false);
    QColor rimColor = light ? QColor(203, 213, 225, 220)
                            : (vp.highContrast ? QColor(255, 255, 255, 200) : QColor(255, 255, 255, 24));
    p.setPen(QPen(rimColor, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(fullRect, winRadius, winRadius);
}

} // namespace mqblox
