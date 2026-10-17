#pragma once
#include "render/Ton618GlWidget.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadialGradient>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <functional>

namespace mqblox {

struct IosSpring {
    double value = 0.0;
    double target = 0.0;
    double velocity = 0.0;
    double stiffness = 330.0;
    double damping = 22.5;

    bool step(double dt = 0.016, bool instant = false, int speedPct = 100, int animStyle = 1) {
        if (instant || animStyle == 2) {
            value = target;
            velocity = 0.0;
            return false;
        }
        double scaledDt = dt * (std::clamp(speedPct, 50, 200) / 100.0);
        double damp = (animStyle == 0) ? 38.0 : damping; // 0 = Плавные без отскока, 1 = Пружинные
        double force = (target - value) * stiffness - velocity * damp;
        velocity += force * scaledDt;
        value += velocity * scaledDt;
        if (std::abs(target - value) < 0.002 && std::abs(velocity) < 0.01) {
            value = target;
            velocity = 0.0;
            return false;
        }
        return true;
    }
};

inline Ton618GlWidget *findBackdropTon618(const QWidget *w) {
    if (!w) return nullptr;
    QWidget *top = w->window();
    return top ? top->findChild<Ton618GlWidget *>() : nullptr;
}

inline QColor blendColors(const QColor &a, const QColor &b, double t, int alpha = 255) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(
        int(a.red() * (1.0 - t) + b.red() * t),
        int(a.green() * (1.0 - t) + b.green() * t),
        int(a.blue() * (1.0 - t) + b.blue() * t),
        std::clamp(alpha, 0, 255)
    );
}

inline void drawHyperOs4LiquidGlass(QPainter &p,
                                    const QWidget *widget,
                                    const QRectF &rect,
                                    double baseRadius,
                                    double hoverMix,
                                    const QPointF &cursorPos,
                                    bool hasCursor,
                                    const QColor &customAccent = QColor(),
                                    double glassOpacityOverride = -1.0) {
    if (rect.width() <= 4 || rect.height() <= 4) return;

    Ton618GlWidget *bg = findBackdropTon618(widget);
    VisualParams vp = bg ? bg->visuals() : VisualParams{};

    const bool isDisabled = widget && !widget->isEnabled();
    const bool hasFocus = widget && widget->hasFocus();
    const bool allowCursorLight = hasCursor && !isDisabled && vp.cursorGlowFollow && !vp.reducedMotion;

    double radius = baseRadius;
    if (baseRadius >= 18.0) {
        if (vp.cornerRadiusMode == 0) radius = 12.0;
        else if (vp.cornerRadiusMode == 2) radius = 28.0;
        else radius = 21.0;
    } else {
        if (vp.cornerRadiusMode == 0) radius = std::min(baseRadius, 8.0);
        else if (vp.cornerRadiusMode == 2) radius = std::min(rect.height() * 0.49, 18.0);
    }

    double userOpacity = vp.glassOpacityPct / 100.0;
    if (vp.glassStyleMode == 1) userOpacity = std::max(userOpacity, 0.22); // Матовое
    else if (vp.glassStyleMode == 2) userOpacity = std::max(userOpacity, 0.34); // Тонированное

    double glassOpacity = (glassOpacityOverride >= 0.0) ? glassOpacityOverride : userOpacity;
    double borderScale = vp.borderBrightnessPct / 100.0;
    double specScale = vp.specularStrengthPct / 100.0;
    double shadowScale = vp.shadowStrengthPct / 100.0;
    double penWidth = std::clamp(vp.borderThicknessPx / 10.0, 0.5, 2.5);

    if (isDisabled) {
        hoverMix = 0.0;
        borderScale *= 0.45;
    }

    QColor cTL = bg ? bg->sampleAdaptiveColor(widget, rect.topLeft()) : QColor(251, 191, 36);
    QColor cMid = bg ? bg->sampleAdaptiveColor(widget, rect.center()) : QColor(56, 189, 248);
    QColor cBR = bg ? bg->sampleAdaptiveColor(widget, rect.bottomRight()) : QColor(168, 85, 247);

    if (customAccent.isValid()) {
        double mixAmt = std::max(0.28, 0.50 * hoverMix);
        cTL = blendColors(cTL, customAccent, mixAmt);
        cMid = blendColors(cMid, customAccent, mixAmt);
        cBR = blendColors(cBR, customAccent, mixAmt);
    }

    // 1. Мягкая внешняя тень
    if (shadowScale > 0.02) {
        p.save();
        p.setPen(Qt::NoPen);
        int shAlpha = std::clamp(int((50 + 28 * hoverMix) * shadowScale), 0, 180);
        p.setBrush(QColor(0, 0, 0, shAlpha));
        p.drawRoundedRect(rect.translated(0, 2.0), radius, radius);
        p.restore();
    }

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);

    p.save();
    p.setClipPath(clipPath);

    // 2. Размытая подложка из общего буфера фона
    if (bg && !vp.highContrast) {
        const QPixmap &frosted = bg->frostedBackdrop();
        if (!frosted.isNull()) {
            QPoint posInBg = widget->mapTo(bg, rect.topLeft().toPoint());
            int shiftX = allowCursorLight ? int((cursorPos.x() - rect.center().x()) * 0.035) : 0;
            int shiftY = allowCursorLight ? int((cursorPos.y() - rect.center().y()) * 0.035) : 2;
            QRect srcRect(posInBg.x() + shiftX, posInBg.y() + shiftY,
                          int(rect.width()), int(rect.height()));
            srcRect = srcRect.intersected(frosted.rect());
            if (srcRect.isValid()) {
                double frostOp = (vp.glassStyleMode == 1)
                    ? std::clamp(0.55 + glassOpacity * 0.7, 0.35, 0.92)
                    : std::clamp(0.36 + glassOpacity * 0.9 + 0.10 * hoverMix, 0.22, 0.85);
                p.setOpacity(frostOp);
                p.drawPixmap(rect.topLeft(), frosted, srcRect);
                p.setOpacity(1.0);
            }
        }
    }

    // 3. Тонирующая вуаль с защитой контраста текста (Тёмная / Светлая / Высокий контраст)
    QLinearGradient tintVeil(rect.topLeft(), rect.bottomRight());
    if (vp.highContrast) {
        tintVeil.setColorAt(0.0, QColor(4, 6, 12, 235));
        tintVeil.setColorAt(1.0, QColor(8, 12, 22, 240));
    } else if (vp.themeMode == 1) {
        int aLight = std::clamp(int(175 + glassOpacity * 120), 165, 240);
        tintVeil.setColorAt(0.0, QColor(255, 255, 255, aLight));
        tintVeil.setColorAt(1.0, QColor(241, 245, 249, aLight));
    } else {
        int lumBg = (cMid.red() * 77 + cMid.green() * 150 + cMid.blue() * 29) >> 8;
        int contrastGuard = (lumBg > 155) ? 42 : 16;
        int aBase = std::clamp(int(glassOpacity * 255 + contrastGuard + 12 * hoverMix), 18, 215);
        int div = (vp.glassStyleMode == 2) ? 4 : 7;
        tintVeil.setColorAt(0.0, QColor(10 + cTL.red() / div, 14 + cTL.green() / div, 26 + cTL.blue() / div, aBase));
        tintVeil.setColorAt(0.5, QColor(6 + cMid.red() / (div + 1), 10 + cMid.green() / (div + 1), 20 + cMid.blue() / (div + 1), int(aBase * 0.88)));
        tintVeil.setColorAt(1.0, QColor(8 + cBR.red() / div, 12 + cBR.green() / div, 24 + cBR.blue() / div, std::min(255, aBase + 8)));
    }
    p.fillRect(rect, tintVeil);

    // 4. Световой прожектор за курсором
    if (allowCursorLight && specScale > 0.02) {
        QColor cCursor = bg ? bg->sampleAdaptiveColor(widget, cursorPos) : cMid;
        double spotR = std::max(rect.width(), rect.height()) * 0.68;
        QRadialGradient spot(cursorPos, spotR);
        int s1 = std::clamp(int((56 + 40 * hoverMix) * specScale), 0, 220);
        int s2 = std::clamp(int((22 + 22 * hoverMix) * specScale), 0, 160);
        spot.setColorAt(0.0, blendColors(cCursor, QColor(255, 255, 255), 0.60, s1));
        spot.setColorAt(0.4, QColor(cCursor.red(), cCursor.green(), cCursor.blue(), s2));
        spot.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(rect, spot);
    }

    // 5. Внутренний купольный блик (Specular Crown)
    if (specScale > 0.02 && !vp.highContrast) {
        QLinearGradient crown(0, rect.top(), 0, rect.top() + rect.height() * 0.36);
        int c1 = std::clamp(int((38 + 34 * hoverMix) * specScale), 0, 220);
        int c2 = std::clamp(int((10 + 12 * hoverMix) * specScale), 0, 150);
        crown.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.70, c1));
        crown.setColorAt(0.45, blendColors(cMid, QColor(255, 255, 255), 0.55, c2));
        crown.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(QRectF(rect.left(), rect.top(), rect.width(), rect.height() * 0.36), crown);
    }

    p.restore();

    // 6. Внутренняя преломляющая кромка
    QRectF innerLip = rect.adjusted(1.1, 1.1, -1.1, -1.1);
    QLinearGradient lipGrad(innerLip.topLeft(), innerLip.bottomRight());
    lipGrad.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.55, std::clamp(int((28 + 36 * hoverMix) * borderScale), 0, 255)));
    lipGrad.setColorAt(1.0, QColor(cBR.red(), cBR.green(), cBR.blue(), std::clamp(int((14 + 22 * hoverMix) * borderScale), 0, 255)));
    p.setPen(QPen(QBrush(lipGrad), 0.75));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(innerLip, std::max(1.0, radius - 1.1), std::max(1.0, radius - 1.1));

    // 7. Градиентная рамка (светлее сверху-слева, темнее снизу-справа)
    QPointF gradStart = allowCursorLight
        ? QPointF(rect.left() * 0.65 + cursorPos.x() * 0.35,
                  rect.top() * 0.65 + cursorPos.y() * 0.35)
        : rect.topLeft();
    QLinearGradient rimGrad(gradStart, rect.bottomRight());
    int aTopLeft = std::clamp(int((115 + 75 * hoverMix) * borderScale), 20, 255);
    int aMid = std::clamp(int((62 + 58 * hoverMix) * borderScale), 15, 245);
    int aBotRight = std::clamp(int((36 + 44 * hoverMix) * borderScale), 10, 220);

    if (vp.highContrast) {
        rimGrad.setColorAt(0.0, QColor(255, 255, 255, 250));
        rimGrad.setColorAt(1.0, QColor(56, 189, 248, 240));
    } else {
        rimGrad.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.55, aTopLeft));
        rimGrad.setColorAt(0.5, QColor(cMid.red(), cMid.green(), cMid.blue(), aMid));
        rimGrad.setColorAt(1.0, blendColors(cBR, QColor(15, 23, 42), 0.35, aBotRight));
    }

    p.setPen(QPen(QBrush(rimGrad), hasFocus ? std::max(1.5, penWidth + 0.4) : penWidth));
    p.drawRoundedRect(rect, radius, radius);

    if (hasFocus && !isDisabled) {
        QColor focusCol = bg ? bg->fixedAccentColor() : QColor(56, 189, 248);
        focusCol.setAlpha(200);
        p.setPen(QPen(focusCol, 1.4, Qt::SolidLine));
        p.drawRoundedRect(rect.adjusted(-0.8, -0.8, 0.8, 0.8), radius + 0.8, radius + 0.8);
    }
}

// Слайдер с дебаунсом 65 мс, числовым индикатором и сбросом к значению по умолчанию по ДВОЙНОМУ КЛИКУ
class ResettableSlider : public QWidget {
    Q_OBJECT
public:
    QSlider *slider = nullptr;
    QLabel *valLabel = nullptr;
    QTimer *debounceTimer = nullptr;
    int defaultVal = 0;
    QString suffixStr;
    double displayMultiplier = 1.0;
    std::function<void(int)> onValueCommitted;

    ResettableSlider(int minV, int maxV, int defV, const QString &suffix = "%",
                     double mult = 1.0, QWidget *parent = nullptr)
        : QWidget(parent), defaultVal(defV), suffixStr(suffix), displayMultiplier(mult) {
        setMinimumWidth(120);
        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);

        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(minV, maxV);
        slider->setValue(defV);
        slider->setCursor(Qt::PointingHandCursor);
        slider->setToolTip(QObject::tr("Двойной клик — сбросить к значению по умолчанию (%1%2)")
                               .arg( mult == 0.1 ? QString::number(defV * 0.1, 'f', 1) : QString::number(defV) )
                               .arg(suffix));
        slider->installEventFilter(this);

        valLabel = new QLabel(this);
        valLabel->setFixedWidth(58);
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valLabel->setToolTip(slider->toolTip());
        valLabel->setCursor(Qt::PointingHandCursor);
        valLabel->installEventFilter(this);

        debounceTimer = new QTimer(this);
        debounceTimer->setSingleShot(true);
        connect(debounceTimer, &QTimer::timeout, this, [this]() {
            if (onValueCommitted) onValueCommitted(slider->value());
        });

        connect(slider, &QSlider::valueChanged, this, [this](int v) {
            updateLabel(v);
            debounceTimer->start(65);
        });

        lay->addWidget(slider, 1);
        lay->addWidget(valLabel);
        updateLabel(defV);
    }

    void setValueSilent(int v) {
        slider->blockSignals(true);
        slider->setValue(v);
        slider->blockSignals(false);
        updateLabel(v);
    }

    int value() const { return slider->value(); }

    void updateLabel(int v) {
        if (std::abs(displayMultiplier - 0.1) < 0.001) {
            valLabel->setText(QString("%1%2").arg(v * 0.1, 0, 'f', 1).arg(suffixStr));
        } else {
            valLabel->setText(QString("%1%2").arg(v).arg(suffixStr));
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if ((watched == slider || watched == valLabel) && event->type() == QEvent::MouseButtonDblClick) {
            slider->setValue(defaultVal);
            if (onValueCommitted) onValueCommitted(defaultVal);
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }
};

class AnimButton : public QPushButton {
    Q_OBJECT
public:
    IosSpring hoverSpring{0.0, 0.0, 0.0, 320.0, 22.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 410.0, 19.5};
    QColor accent;
    bool isPlay = false, isDanger = false;
    bool hasMouse = false;
    QPointF mousePos;
    QTimer *physTimer = nullptr;

    explicit AnimButton(const QString &text,
                        QWidget *parent = nullptr,
                        QColor acc = QColor(),
                        bool play = false,
                        bool danger = false)
        : QPushButton(text, parent), accent(acc), isPlay(play), isDanger(danger) {
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setMinimumHeight(36);
        setMinimumWidth(60);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            Ton618GlWidget *bg = findBackdropTon618(this);
            VisualParams vp = bg ? bg->visuals() : VisualParams{};
            bool a1 = hoverSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a2 = scaleSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void setAccentColor(const QColor &c, bool pulse = false) {
        accent = c;
        if (pulse) {
            scaleSpring.velocity += 2.0;
            hoverSpring.value = 0.85;
            if (!physTimer->isActive()) physTimer->start(16);
        }
        update();
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.022;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::enterEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        mousePos = e->position();
        Ton618GlWidget *bg = findBackdropTon618(this);
        if (!bg || !bg->isReducedMotion()) {
            if (!physTimer->isActive()) physTimer->start(16);
        }
        QPushButton::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false; hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.93;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = hasMouse ? 1.022 : 1.0;
        scaleSpring.velocity += 2.4;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double sc = isEnabled() ? scaleSpring.value : 1.0;

        p.save();
        p.translate(w * 0.5, h * 0.5);
        p.scale(sc, sc);
        p.translate(-w * 0.5, -h * 0.5);

        QRectF r(1.5, 1.5, w - 3.0, h - 3.0);
        double rad = std::min(r.height() * 0.48, 14.0);

        if (isPlay) {
            drawHyperOs4LiquidGlass(p, this, r, rad, 0.55 + 0.45 * hp, mousePos, hasMouse, QColor(52, 211, 153));
        } else if (isDanger) {
            drawHyperOs4LiquidGlass(p, this, r, rad, 0.45 + 0.55 * hp, mousePos, hasMouse, QColor(248, 113, 113));
        } else {
            drawHyperOs4LiquidGlass(p, this, r, rad, hp, mousePos, hasMouse, accent);
        }

        Ton618GlWidget *bg = findBackdropTon618(this);
        QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
        p.setPen(isEnabled() ? txtCol : QColor(148, 163, 184, 155));
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
        p.drawText(r, Qt::AlignCenter, text());
        p.restore();
    }
};

class AnimNavButton : public QPushButton {
    Q_OBJECT
public:
    QString iconStr, titleStr;
    bool showTitle = true, isActiveTab = false;
    IosSpring hoverSpring{0.0, 0.0, 0.0, 300.0, 21.0};
    IosSpring activeSpring{0.0, 0.0, 0.0, 270.0, 19.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 380.0, 19.0};
    QPointF mousePos;
    bool hasMouse = false;
    QTimer *physTimer = nullptr;

    AnimNavButton(const QString &ico, const QString &title, QWidget *parent = nullptr)
        : QPushButton(parent), iconStr(ico), titleStr(title) {
        setFixedHeight(42);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            Ton618GlWidget *bg = findBackdropTon618(this);
            VisualParams vp = bg ? bg->visuals() : VisualParams{};
            bool a1 = hoverSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a2 = activeSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a3 = scaleSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            update();
            if (!a1 && !a2 && !a3) physTimer->stop();
        });
    }

    void setActiveTab(bool act) {
        isActiveTab = act;
        activeSpring.target = act ? 1.0 : 0.0;
        if (act) scaleSpring.velocity += 1.4;
        if (!physTimer->isActive()) physTimer->start(16);
    }
    void setCompactMode(bool compact) { showTitle = !compact; update(); }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.02;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::enterEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        mousePos = e->position();
        Ton618GlWidget *bg = findBackdropTon618(this);
        if (!bg || !bg->isReducedMotion()) {
            if (!physTimer->isActive()) physTimer->start(16);
        }
        QPushButton::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false; hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.94;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = hasMouse ? 1.02 : 1.0;
        scaleSpring.velocity += 2.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double ap = std::clamp(activeSpring.value, 0.0, 1.0);
        double mix = std::max(hp * 0.78, ap);
        double sc = scaleSpring.value;

        p.save();
        p.translate(w * 0.5, h * 0.5);
        p.scale(sc, sc);
        p.translate(-w * 0.5, -h * 0.5);

        QRectF r(1.5, 1.5, w - 3.0, h - 3.0);
        drawHyperOs4LiquidGlass(p, this, r, 13.5, mix, mousePos, hasMouse);

        Ton618GlWidget *bg = findBackdropTon618(this);
        if (mix > 0.04) {
            QColor pillCol = bg ? bg->sampleAdaptiveColor(this, QPointF(8.0, h * 0.5)).lighter(125) : QColor(255, 255, 255);
            pillCol.setAlpha(int(245 * mix));
            double pillH = (h - 18.0) * mix;
            p.setPen(Qt::NoPen);
            p.setBrush(pillCol);
            p.drawRoundedRect(QRectF(6.5, (h - pillH) * 0.5, 3.6, pillH), 1.8, 1.8);
        }

        QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false);
        if (showTitle) {
            f.setPixelSize(13); f.setBold(isActiveTab || hp > 0.25); p.setFont(f);
            p.setPen(txtCol);
            double sx = 16.0 + 4.0 * hp;
            p.drawText(QRectF(sx, 0, w - sx - 6.0, h), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1   %2").arg(iconStr, titleStr));
        } else {
            f.setPixelSize(17); p.setFont(f); p.setPen(txtCol);
            p.drawText(r, Qt::AlignCenter, iconStr);
        }
        p.restore();
    }
};

class DarkComboBox : public QComboBox {
    Q_OBJECT
public:
    IosSpring hoverSpring{0.0, 0.0, 0.0, 310.0, 22.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 390.0, 20.0};
    QPointF mousePos;
    bool hasMouse = false;
    QTimer *physTimer = nullptr;

    explicit DarkComboBox(QWidget *parent = nullptr) : QComboBox(parent) {
        setMinimumHeight(36);
        setMinimumWidth(60);
        setMouseTracking(true);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(4);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto *lv = new QListView(this);
        lv->setObjectName("ComboPopupList");
        setView(lv);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            Ton618GlWidget *bg = findBackdropTon618(this);
            VisualParams vp = bg ? bg->visuals() : VisualParams{};
            bool a1 = hoverSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a2 = scaleSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void showPopup() override {
        scaleSpring.velocity -= 1.6;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::showPopup();
        if (view() && view()->window()) {
            QWidget *popWin = view()->window();
            popWin->setAttribute(Qt::WA_TranslucentBackground);
            Ton618GlWidget *bg = findBackdropTon618(this);
            QColor rimC = bg ? bg->sampleAdaptiveColor(this, rect().center()) : QColor(255, 255, 255);
            popWin->setStyleSheet(QString(
                "background-color: rgba(8, 12, 24, 195); "
                "border: 1px solid rgba(%1, %2, %3, 155); "
                "border-radius: 14px;").arg(rimC.red()).arg(rimC.green()).arg(rimC.blue()));
            if (!bg || !bg->isReducedMotion()) {
                auto *fade = new QPropertyAnimation(popWin, "windowOpacity", popWin);
                fade->setDuration(160);
                fade->setStartValue(0.0);
                fade->setEndValue(1.0);
                fade->setEasingCurve(QEasingCurve::OutCubic);
                fade->start(QAbstractAnimation::DeleteWhenStopped);
            }
        }
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.014;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::enterEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        mousePos = e->position();
        Ton618GlWidget *bg = findBackdropTon618(this);
        if (!bg || !bg->isReducedMotion()) {
            if (!physTimer->isActive()) physTimer->start(16);
        }
        QComboBox::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false; hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.96;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = 1.0; scaleSpring.velocity += 1.6;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double sc = scaleSpring.value;

        p.save();
        p.translate(w * 0.5, h * 0.5);
        p.scale(sc, sc);
        p.translate(-w * 0.5, -h * 0.5);

        QRectF r(1.5, 1.5, w - 3.0, h - 3.0);
        drawHyperOs4LiquidGlass(p, this, r, 13.0, hp, mousePos, hasMouse);

        Ton618GlWidget *bg = findBackdropTon618(this);
        QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
        p.setPen(isEnabled() ? txtCol : QColor(148, 163, 184, 155));
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
        QString elided = p.fontMetrics().elidedText(currentText(), Qt::ElideRight, std::max(20, int(w - 52.0)));
        p.drawText(QRectF(14.0, 0, w - 46.0, h), Qt::AlignVCenter | Qt::AlignLeft, elided);

        QColor badgeCol = bg ? bg->sampleAdaptiveColor(this, QPointF(w - 19.5, h * 0.5)) : QColor(255, 255, 255);

        double badgeR = 10.5;
        QPointF badgeCenter(w - 19.5, h * 0.5);
        p.setPen(QPen(QColor(badgeCol.red(), badgeCol.green(), badgeCol.blue(), int(85 + 85 * hp)), 0.9));
        p.setBrush(QColor(badgeCol.red(), badgeCol.green(), badgeCol.blue(), int(22 + 35 * hp)));
        p.drawEllipse(badgeCenter, badgeR, badgeR);

        QPainterPath chev;
        double cx = badgeCenter.x(), cy = badgeCenter.y();
        chev.moveTo(cx - 3.0, cy - 1.2); chev.lineTo(cx, cy - 4.0); chev.lineTo(cx + 3.0, cy - 1.2);
        chev.moveTo(cx - 3.0, cy + 1.2); chev.lineTo(cx, cy + 4.0); chev.lineTo(cx + 3.0, cy + 1.2);

        p.setPen(QPen(txtCol, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(chev);
        p.restore();
    }
};

class HoverSidebar : public QFrame {
    Q_OBJECT
public:
    std::function<void(bool)> onHoverChanged;
    explicit HoverSidebar(QWidget *parent = nullptr) : QFrame(parent) {}
protected:
    void enterEvent(QEnterEvent *e) override { if (onHoverChanged) onHoverChanged(true); QFrame::enterEvent(e); }
    void leaveEvent(QEvent *e) override { if (onHoverChanged) onHoverChanged(false); QFrame::leaveEvent(e); }
};

class LiquidGlassEngine : public QObject {
    Q_OBJECT
public:
    struct SurfaceState {
        IosSpring hover{0.0, 0.0, 0.0, 260.0, 22.0};
        QPointF cursor;
        bool hasCursor = false;
        bool mouseDirty = false;
    };
    struct SwitchState {
        IosSpring pos{0.0, 0.0, 0.0, 330.0, 21.0};
        IosSpring pressStretch{0.0, 0.0, 0.0, 390.0, 22.0};
        IosSpring hover{0.0, 0.0, 0.0, 280.0, 22.0};
        bool initialized = false;
    };

    QHash<QWidget *, SurfaceState> surfaces;
    QHash<QCheckBox *, SwitchState> switches;
    QTimer *tickTimer = nullptr;

    explicit LiquidGlassEngine(QObject *parent = nullptr) : QObject(parent) {
        tickTimer = new QTimer(this);
        connect(tickTimer, &QTimer::timeout, this, [this]() {
            bool anyActive = false;
            const auto surfKeys = surfaces.keys();
            for (QWidget *k : surfKeys) {
                auto it = surfaces.find(k);
                if (it != surfaces.end()) {
                    Ton618GlWidget *bg = findBackdropTon618(k);
                    VisualParams vp = bg ? bg->visuals() : VisualParams{};
                    bool stepped = it.value().hover.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    if (stepped || it.value().mouseDirty) {
                        it.value().mouseDirty = false;
                        if (stepped) anyActive = true;
                        k->update();
                    }
                }
            }
            const auto swKeys = switches.keys();
            for (QCheckBox *k : swKeys) {
                auto it = switches.find(k);
                if (it != switches.end()) {
                    Ton618GlWidget *bg = findBackdropTon618(k);
                    VisualParams vp = bg ? bg->visuals() : VisualParams{};
                    bool s1 = it.value().pos.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    bool s2 = it.value().pressStretch.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    bool s3 = it.value().hover.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    if (s1 || s2 || s3) { anyActive = true; k->update(); }
                }
            }
            if (!anyActive) tickTimer->stop();
        });
    }

    static bool isGlassContainer(QObject *obj, QFrame **outFrame) {
        auto *fr = qobject_cast<QFrame *>(obj);
        if (!fr) return false;
        const QString name = fr->objectName();
        if (name == "GlassCard" || name == "Sidebar" || name == "AuthorsBox" || name == "WarningBanner") {
            *outFrame = fr;
            return true;
        }
        return false;
    }

    static bool isGlassBadgeLabel(QObject *obj, QLabel **outLbl) {
        auto *lbl = qobject_cast<QLabel *>(obj);
        if (!lbl) return false;
        const QString name = lbl->objectName();
        if (name == "RobuxCardBadge" || name == "AutoSyncBadge" || name == "StatusBadge" || name == "ToastBanner") {
            *outLbl = lbl;
            return true;
        }
        return false;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        QFrame *frame = nullptr;
        if (isGlassContainer(watched, &frame)) {
            if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
                frame->setMouseTracking(true);
                connect(frame, &QObject::destroyed, this, [this, frame]() { surfaces.remove(frame); });
            } else if (event->type() == QEvent::Enter) {
                auto &st = surfaces[frame];
                st.hasCursor = true; st.hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseMove) {
                auto *me = static_cast<QMouseEvent *>(event);
                auto &st = surfaces[frame];
                st.hasCursor = true; st.cursor = me->position();
                Ton618GlWidget *bg = findBackdropTon618(frame);
                if (!bg || !bg->isReducedMotion()) {
                    st.mouseDirty = true;
                    if (!tickTimer->isActive()) tickTimer->start(16);
                }
            } else if (event->type() == QEvent::Leave) {
                auto &st = surfaces[frame];
                st.hasCursor = false; st.hover.target = 0.0; st.mouseDirty = true;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(frame);
                p.setRenderHint(QPainter::Antialiasing);
                auto &st = surfaces[frame];
                const QString name = frame->objectName();
                QRectF r(1.5, 1.5, frame->width() - 3.0, frame->height() - 3.0);

                double rad = (name == "GlassCard" || name == "Sidebar") ? 21.0 : 14.0;
                drawHyperOs4LiquidGlass(p, frame, r, rad, std::clamp(st.hover.value, 0.0, 1.0),
                                        st.cursor, st.hasCursor);
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *secLbl = qobject_cast<QLabel *>(watched)) {
            if (secLbl->objectName() == "SectionTitle" && event->type() == QEvent::Paint) {
                QPainter p(secLbl);
                p.setRenderHint(QPainter::Antialiasing);
                Ton618GlWidget *bg = findBackdropTon618(secLbl);
                QColor c1 = bg ? bg->sampleAdaptiveColor(secLbl, QPointF(0, 0)).lighter(135) : QColor(253, 224, 71);
                QColor c2 = bg ? bg->sampleAdaptiveColor(secLbl, QPointF(secLbl->width(), 0)).lighter(135) : QColor(125, 211, 252);
                QLinearGradient tg(0, 0, secLbl->width(), 0);
                QColor baseTxt = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
                tg.setColorAt(0.0, blendColors(c1, baseTxt, 0.35));
                tg.setColorAt(1.0, blendColors(c2, baseTxt, 0.35));
                QFont f = secLbl->font();
                f.setStyle(QFont::StyleNormal);
                f.setItalic(false);
                f.setPixelSize(15);
                f.setBold(true);
                p.setFont(f);
                p.setPen(QPen(QBrush(tg), 1.0));
                secLbl->setMinimumWidth(0);
                QString secTxt = p.fontMetrics().elidedText(secLbl->text(), Qt::ElideRight, std::max(20, secLbl->width() - 4));
                p.drawText(secLbl->rect(), Qt::AlignVCenter | Qt::AlignLeft, secTxt);
                return true;
            }
        }

        if (qobject_cast<QLineEdit *>(watched) || qobject_cast<QSpinBox *>(watched)) {
            auto *w = static_cast<QWidget *>(watched);
            if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
                w->setMouseTracking(true);
                connect(w, &QObject::destroyed, this, [this, w]() { surfaces.remove(w); });
            } else if (event->type() == QEvent::Enter) {
                surfaces[w].hasCursor = true; surfaces[w].hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseMove) {
                surfaces[w].cursor = static_cast<QMouseEvent *>(event)->position();
                Ton618GlWidget *bg = findBackdropTon618(w);
                if (!bg || !bg->isReducedMotion()) {
                    surfaces[w].mouseDirty = true;
                    if (!tickTimer->isActive()) tickTimer->start(16);
                }
            } else if (event->type() == QEvent::Leave) {
                surfaces[w].hasCursor = false; surfaces[w].hover.target = 0.0; surfaces[w].mouseDirty = true;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(w);
                p.setRenderHint(QPainter::Antialiasing);
                auto &st = surfaces[w];
                double mix = w->hasFocus() ? 1.0 : std::clamp(st.hover.value, 0.0, 1.0);
                QRectF r(1.5, 1.5, w->width() - 3.0, w->height() - 3.0);
                drawHyperOs4LiquidGlass(p, w, r, 12.5, mix, st.cursor, st.hasCursor);
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        QLabel *badgeLbl = nullptr;
        if (isGlassBadgeLabel(watched, &badgeLbl)) {
            if (event->type() == QEvent::Paint) {
                QPainter p(badgeLbl);
                p.setRenderHint(QPainter::Antialiasing);
                QRectF r(1.0, 1.0, badgeLbl->width() - 2.0, badgeLbl->height() - 2.0);
                drawHyperOs4LiquidGlass(p, badgeLbl, r, std::min(r.height() * 0.48, 12.0),
                                        0.45, r.center(), false);
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *bar = qobject_cast<QProgressBar *>(watched)) {
            if (event->type() == QEvent::Paint) {
                QPainter p(bar);
                p.setRenderHint(QPainter::Antialiasing);
                QRectF r(1.0, 1.0, bar->width() - 2.0, bar->height() - 2.0);
                double rad = r.height() * 0.48;
                drawHyperOs4LiquidGlass(p, bar, r, rad, 0.30, r.center(), false);

                double frac = 1.0;
                if (bar->maximum() > bar->minimum()) {
                    frac = std::clamp(double(bar->value() - bar->minimum()) / (bar->maximum() - bar->minimum()), 0.0, 1.0);
                }
                Ton618GlWidget *bg = findBackdropTon618(bar);
                if (frac > 0.02) {
                    QColor cL = bg ? bg->sampleAdaptiveColor(bar, r.topLeft()) : QColor(56, 189, 248);
                    QColor cR = bg ? bg->sampleAdaptiveColor(bar, r.topRight()) : QColor(251, 191, 36);
                    cL.setAlpha(165);
                    cR.setAlpha(180);

                    QRectF fillR = r.adjusted(1.5, 1.5, -1.5, -1.5);
                    fillR.setWidth(fillR.width() * frac);
                    QLinearGradient fg(fillR.topLeft(), fillR.topRight());
                    fg.setColorAt(0.0, cL);
                    fg.setColorAt(1.0, cR);
                    p.setPen(QPen(QColor(255, 255, 255, 130), 0.7));
                    p.setBrush(fg);
                    p.drawRoundedRect(fillR, rad - 1.5, rad - 1.5);
                }
                p.setPen(bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255));
                QFont f = bar->font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
                p.drawText(r, Qt::AlignCenter, bar->text());
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *chk = qobject_cast<QCheckBox *>(watched)) {
            auto &st = switches[chk];
            if (!st.initialized) {
                st.initialized = true;
                st.pos.value = st.pos.target = chk->isChecked() ? 1.0 : 0.0;
                chk->setMinimumWidth(0);
                chk->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
                chk->setCursor(Qt::PointingHandCursor);
                connect(chk, &QCheckBox::toggled, this, [this, chk](bool c) {
                    auto &s = switches[chk];
                    s.pos.target = c ? 1.0 : 0.0;
                    s.pos.velocity += c ? 2.5 : -2.5;
                    if (!tickTimer->isActive()) tickTimer->start(16);
                });
                connect(chk, &QObject::destroyed, this, [this, chk]() { switches.remove(chk); });
            }

            if (event->type() == QEvent::Enter) {
                st.hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Leave) {
                st.hover.target = 0.0; st.pressStretch.target = 0.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseButtonPress) {
                st.pressStretch.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseButtonRelease) {
                st.pressStretch.target = 0.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(chk);
                p.setRenderHint(QPainter::Antialiasing);

                double h = chk->height();
                double swW = 48.0, swH = 26.0;
                double swY = (h - swH) * 0.5;
                QRectF trackRect(2.0, swY, swW, swH);

                double tPos = std::clamp(st.pos.value, 0.0, 1.0);
                double stretch = std::clamp(st.pressStretch.value, 0.0, 1.0);
                double hov = std::clamp(st.hover.value, 0.0, 1.0);

                Ton618GlWidget *bg = findBackdropTon618(chk);
                QColor swColor = bg ? bg->sampleAdaptiveColor(chk, trackRect.center()) : QColor(56, 189, 248);

                drawHyperOs4LiquidGlass(p, chk, trackRect, swH * 0.5, std::max(tPos, hov),
                                        trackRect.center(), false, swColor);

                if (tPos > 0.05) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(swColor.red(), swColor.green(), swColor.blue(), int(145 * tPos)));
                    p.drawRoundedRect(trackRect.adjusted(1.0, 1.0, -1.0, -1.0), swH * 0.46, swH * 0.46);
                }

                double thumbBaseD = swH - 4.4;
                double thumbW = thumbBaseD + stretch * 7.0;
                double minX = trackRect.left() + 2.2;
                double maxX = trackRect.right() - 2.2 - thumbW;
                double thumbX = minX + (maxX - minX) * tPos;
                QRectF thumbRect(thumbX, swY + 2.2, thumbW, thumbBaseD);

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 85));
                p.drawRoundedRect(thumbRect.translated(0, 1.2), thumbBaseD * 0.5, thumbBaseD * 0.5);

                QLinearGradient thumbGrad(thumbRect.topLeft(), thumbRect.bottomRight());
                thumbGrad.setColorAt(0.0, QColor(255, 255, 255, chk->isEnabled() ? 250 : 140));
                thumbGrad.setColorAt(1.0, blendColors(swColor, QColor(255, 255, 255), 0.72, chk->isEnabled() ? 235 : 120));
                p.setBrush(thumbGrad);
                p.setPen(QPen(QColor(255, 255, 255, chk->isEnabled() ? 240 : 110), 0.9));
                p.drawRoundedRect(thumbRect, thumbBaseD * 0.5, thumbBaseD * 0.5);

                QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
                p.setPen(chk->isEnabled() ? txtCol : QColor(148, 163, 184, 155));
                QFont f = chk->font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setWeight(QFont::Medium); p.setFont(f);
                QString chkTxt = p.fontMetrics().elidedText(chk->text(), Qt::ElideRight, std::max(20, int(chk->width() - trackRect.right() - 16.0)));
                p.drawText(QRectF(trackRect.right() + 12.0, 0, chk->width() - trackRect.right() - 14.0, h),
                           Qt::AlignVCenter | Qt::AlignLeft, chkTxt);
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

inline void installLiquidGlassEngine(QApplication *app) {
    auto *engine = new LiquidGlassEngine(app);
    app->installEventFilter(engine);
}

} // namespace mqblox
