#pragma once
#include "render/Ton618GlWidget.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFrame>
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

    bool step(double dt = 0.016, bool instant = false) {
        if (instant) {
            value = target;
            velocity = 0.0;
            return false;
        }
        double force = (target - value) * stiffness - velocity * damping;
        velocity += force * dt;
        value += velocity * dt;
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

// МНОГОСЛОЙНЫЙ ШЕЙДЕР HYPEROS 4 LIQUID GLASS v34.0:
// 1) Мягкая внешняя тень глубины
// 2) Единая размытая подложка фона (без дублирования буферов GPU)
// 3) Адаптивная тонировка с гарантией контраста текста WCAG AA
// 4) Внутренний купольный блик + подсветка за курсором
// 5) Градиентная рамка 1.0px (светлее сверху-слева, темнее снизу-справа) + Focus Ring
inline void drawHyperOs4LiquidGlass(QPainter &p,
                                    const QWidget *widget,
                                    const QRectF &rect,
                                    double radius,
                                    double hoverMix,
                                    const QPointF &cursorPos,
                                    bool hasCursor,
                                    const QColor &customAccent = QColor(),
                                    double glassOpacityOverride = -1.0) {
    if (rect.width() <= 4 || rect.height() <= 4) return;

    Ton618GlWidget *bg = findBackdropTon618(widget);
    const bool isDisabled = widget && !widget->isEnabled();
    const bool hasFocus = widget && widget->hasFocus();

    double userOpacity = bg ? (bg->glassOpacityPct() / 100.0) : 0.10;
    double glassOpacity = (glassOpacityOverride >= 0.0) ? glassOpacityOverride : userOpacity;
    double borderScale = bg ? (bg->borderBrightnessPct() / 100.0) : 1.0;

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

    // 1. Мягкая внешняя тень глубины
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, int(52 + 28 * hoverMix)));
    p.drawRoundedRect(rect.translated(0, 2.0), radius, radius);
    p.restore();

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);

    p.save();
    p.setClipPath(clipPath);

    // 2. Размытая подложка из общего буфера фона (0 лишних текстур!)
    if (bg) {
        const QPixmap &frosted = bg->frostedBackdrop();
        if (!frosted.isNull()) {
            QPoint posInBg = widget->mapTo(bg, rect.topLeft().toPoint());
            int shiftX = hasCursor ? int((cursorPos.x() - rect.center().x()) * 0.035) : 0;
            int shiftY = hasCursor ? int((cursorPos.y() - rect.center().y()) * 0.035) : 2;
            QRect srcRect(posInBg.x() + shiftX, posInBg.y() + shiftY,
                          int(rect.width()), int(rect.height()));
            srcRect = srcRect.intersected(frosted.rect());
            if (srcRect.isValid()) {
                p.setOpacity(std::clamp(0.36 + glassOpacity * 0.9 + 0.10 * hoverMix, 0.22, 0.85));
                p.drawPixmap(rect.topLeft(), frosted, srcRect);
                p.setOpacity(1.0);
            }
        }
    }

    // 3. Адаптивная подложка для читаемости текста (WCAG AA)
    int lumBg = (cMid.red() * 77 + cMid.green() * 150 + cMid.blue() * 29) >> 8;
    int contrastGuard = (lumBg > 155) ? 38 : 14;
    int aBase = std::clamp(int(glassOpacity * 255 + contrastGuard + 12 * hoverMix), 18, 145);

    QLinearGradient tintVeil(rect.topLeft(), rect.bottomRight());
    tintVeil.setColorAt(0.0, QColor(10 + cTL.red() / 7, 14 + cTL.green() / 7, 26 + cTL.blue() / 7, aBase));
    tintVeil.setColorAt(0.5, QColor(6 + cMid.red() / 8, 10 + cMid.green() / 8, 20 + cMid.blue() / 8, int(aBase * 0.88)));
    tintVeil.setColorAt(1.0, QColor(8 + cBR.red() / 7, 12 + cBR.green() / 7, 24 + cBR.blue() / 7, aBase + 8));
    p.fillRect(rect, tintVeil);

    // 4. Световой блик, следующий за курсором мыши
    if (hasCursor && !isDisabled) {
        QColor cCursor = bg ? bg->sampleAdaptiveColor(widget, cursorPos) : cMid;
        double spotR = std::max(rect.width(), rect.height()) * 0.68;
        QRadialGradient spot(cursorPos, spotR);
        spot.setColorAt(0.0, blendColors(cCursor, QColor(255, 255, 255), 0.60, int(56 + 40 * hoverMix)));
        spot.setColorAt(0.4, QColor(cCursor.red(), cCursor.green(), cCursor.blue(), int(22 + 22 * hoverMix)));
        spot.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(rect, spot);
    }

    // 5. Тонкий внутренний верхний блик (Specular Sheen)
    QLinearGradient crown(0, rect.top(), 0, rect.top() + rect.height() * 0.36);
    crown.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.70, int((38 + 34 * hoverMix) * borderScale)));
    crown.setColorAt(0.45, blendColors(cMid, QColor(255, 255, 255), 0.55, int((10 + 12 * hoverMix) * borderScale)));
    crown.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(rect.left(), rect.top(), rect.width(), rect.height() * 0.36), crown);

    p.restore();

    // 6. Внутренняя преломляющая кромка (0.75px)
    QRectF innerLip = rect.adjusted(1.1, 1.1, -1.1, -1.1);
    QLinearGradient lipGrad(innerLip.topLeft(), innerLip.bottomRight());
    lipGrad.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.55, std::clamp(int((28 + 36 * hoverMix) * borderScale), 0, 255)));
    lipGrad.setColorAt(1.0, QColor(cBR.red(), cBR.green(), cBR.blue(), std::clamp(int((14 + 22 * hoverMix) * borderScale), 0, 255)));
    p.setPen(QPen(QBrush(lipGrad), 0.75));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(innerLip, std::max(1.0, radius - 1.1), std::max(1.0, radius - 1.1));

    // 7. Градиентная рамка 1.0px (светлее сверху-слева, темнее снизу-справа, с подсветкой у курсора)
    QPointF gradStart = hasCursor ? blendColors(QColor(int(rect.left()), int(rect.top()), 0),
                                                QColor(int(cursorPos.x()), int(cursorPos.y()), 0), 0.35) == QColor()
                                        ? rect.topLeft() : QPointF((rect.left() + cursorPos.x()) * 0.5, (rect.top() + cursorPos.y()) * 0.5)
                                  : rect.topLeft();
    QLinearGradient rimGrad(gradStart, rect.bottomRight());
    int aTopLeft = std::clamp(int((115 + 75 * hoverMix) * borderScale), 20, 255);
    int aMid = std::clamp(int((62 + 58 * hoverMix) * borderScale), 15, 240);
    int aBotRight = std::clamp(int((36 + 44 * hoverMix) * borderScale), 10, 210);

    rimGrad.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.55, aTopLeft));
    rimGrad.setColorAt(0.5, QColor(cMid.red(), cMid.green(), cMid.blue(), aMid));
    rimGrad.setColorAt(1.0, blendColors(cBR, QColor(15, 23, 42), 0.35, aBotRight));

    p.setPen(QPen(QBrush(rimGrad), hasFocus ? 1.5 : 1.0));
    p.drawRoundedRect(rect, radius, radius);

    // 8. Кольцо фокуса при навигации с клавиатуры (Tab Focus-Ring)
    if (hasFocus && !isDisabled) {
        QColor focusCol = bg ? bg->fixedAccentColor() : QColor(56, 189, 248);
        focusCol.setAlpha(190);
        p.setPen(QPen(focusCol, 1.4, Qt::SolidLine));
        p.drawRoundedRect(rect.adjusted(-0.8, -0.8, 0.8, 0.8), radius + 0.8, radius + 0.8);
    }
}

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
            bool instant = bg && bg->isReducedMotion();
            bool a1 = hoverSpring.step(0.016, instant);
            bool a2 = scaleSpring.step(0.016, instant);
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
        mousePos = e->position(); update();
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

        p.setPen(isEnabled() ? QColor(255, 255, 255) : QColor(148, 163, 184, 155));
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
            bool instant = bg && bg->isReducedMotion();
            bool a1 = hoverSpring.step(0.016, instant);
            bool a2 = activeSpring.step(0.016, instant);
            bool a3 = scaleSpring.step(0.016, instant);
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
        mousePos = e->position(); update();
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

        if (mix > 0.04) {
            Ton618GlWidget *bg = findBackdropTon618(this);
            QColor pillCol = bg ? bg->sampleAdaptiveColor(this, QPointF(8.0, h * 0.5)).lighter(125) : QColor(255, 255, 255);
            pillCol.setAlpha(int(245 * mix));
            double pillH = (h - 18.0) * mix;
            p.setPen(Qt::NoPen);
            p.setBrush(pillCol);
            p.drawRoundedRect(QRectF(6.5, (h - pillH) * 0.5, 3.6, pillH), 1.8, 1.8);
        }

        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false);
        if (showTitle) {
            f.setPixelSize(13); f.setBold(isActiveTab || hp > 0.25); p.setFont(f);
            p.setPen(QColor(255, 255, 255));
            double sx = 16.0 + 4.0 * hp;
            p.drawText(QRectF(sx, 0, w - sx - 6.0, h), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1   %2").arg(iconStr, titleStr));
        } else {
            f.setPixelSize(17); p.setFont(f); p.setPen(QColor(255, 255, 255));
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
            bool instant = bg && bg->isReducedMotion();
            bool a1 = hoverSpring.step(0.016, instant);
            bool a2 = scaleSpring.step(0.016, instant);
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
                "background-color: rgba(8, 12, 24, 185); "
                "border: 1px solid rgba(%1, %2, %3, 145); "
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
        mousePos = e->position(); update();
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

        p.setPen(isEnabled() ? QColor(255, 255, 255) : QColor(148, 163, 184, 155));
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
        QString elided = p.fontMetrics().elidedText(currentText(), Qt::ElideRight, std::max(20, int(w - 52.0)));
        p.drawText(QRectF(14.0, 0, w - 46.0, h), Qt::AlignVCenter | Qt::AlignLeft, elided);

        Ton618GlWidget *bg = findBackdropTon618(this);
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

        p.setPen(QPen(QColor(255, 255, 255, 240), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
                    bool instant = bg && bg->isReducedMotion();
                    if (it.value().hover.step(0.016, instant)) {
                        anyActive = true;
                        k->update();
                    }
                }
            }
            const auto swKeys = switches.keys();
            for (QCheckBox *k : swKeys) {
                auto it = switches.find(k);
                if (it != switches.end()) {
                    Ton618GlWidget *bg = findBackdropTon618(k);
                    bool instant = bg && bg->isReducedMotion();
                    bool s1 = it.value().pos.step(0.016, instant);
                    bool s2 = it.value().pressStretch.step(0.016, instant);
                    bool s3 = it.value().hover.step(0.016, instant);
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
                frame->update();
            } else if (event->type() == QEvent::Leave) {
                auto &st = surfaces[frame];
                st.hasCursor = false; st.hover.target = 0.0;
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
                tg.setColorAt(0.0, blendColors(c1, QColor(255, 255, 255), 0.35));
                tg.setColorAt(1.0, blendColors(c2, QColor(255, 255, 255), 0.35));
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
                w->update();
            } else if (event->type() == QEvent::Leave) {
                surfaces[w].hasCursor = false; surfaces[w].hover.target = 0.0;
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
                if (frac > 0.02) {
                    Ton618GlWidget *bg = findBackdropTon618(bar);
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
                p.setPen(QColor(255, 255, 255));
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

                p.setPen(chk->isEnabled() ? QColor(255, 255, 255) : QColor(148, 163, 184, 155));
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
