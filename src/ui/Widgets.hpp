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

    bool step(double dt = 0.016) {
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

// ПОЛНЫЙ ОПТИЧЕСКИЙ ШЕЙДЕР HYPEROS 4 LIQUID GLASS (С преломлением живого видео, объёмной фаской и каустическим бликом)
inline void drawHyperOs4LiquidGlass(QPainter &p,
                                    const QWidget *widget,
                                    const QRectF &rect,
                                    double radius,
                                    double hoverMix,
                                    const QPointF &cursorPos,
                                    bool hasCursor,
                                    const QColor &tintColor = QColor(56, 189, 248),
                                    double glassOpacity = 0.22) {
    if (rect.width() <= 4 || rect.height() <= 4) return;

    // 1. Мягкая тень парения под стеклянной капсулой
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, int(80 + 40 * hoverMix)));
    p.drawRoundedRect(rect.translated(0, 2.4), radius, radius);
    p.restore();

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);

    p.save();
    p.setClipPath(clipPath);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 2. Оптическое преломление живого видео-фона чёрной дыры
    if (Ton618GlWidget *bg = findBackdropTon618(widget)) {
        const QPixmap &frosted = bg->frostedBackdrop();
        if (!frosted.isNull() && bg->width() > 0 && bg->height() > 0) {
            QPoint topLeftInBg = widget->mapTo(bg, rect.topLeft().toPoint());
            double sxRatio = double(frosted.width()) / bg->width();
            double syRatio = double(frosted.height()) / bg->height();

            double shiftX = hasCursor ? (cursorPos.x() - rect.center().x()) * 0.045 : 0.0;
            double shiftY = hasCursor ? (cursorPos.y() - rect.center().y()) * 0.045 : 0.0;

            QRectF srcRect(
                (topLeftInBg.x() + shiftX) * sxRatio,
                (topLeftInBg.y() + shiftY) * syRatio,
                rect.width() * sxRatio,
                rect.height() * syRatio
            );
            srcRect = srcRect.intersected(QRectF(frosted.rect()));
            if (srcRect.isValid()) {
                p.drawPixmap(rect, frosted, srcRect);
            }
        }
    }

    // 3. Полупрозрачный слой жидкого акрила HyperOS 4
    QLinearGradient acrylic(rect.topLeft(), rect.bottomRight());
    int aTop = std::clamp(int((glassOpacity + 0.05 * hoverMix) * 255), 28, 215);
    int aBot = std::clamp(int((glassOpacity + 0.10 + 0.05 * hoverMix) * 255), 42, 225);
    acrylic.setColorAt(0.0, QColor(18, 25, 46, aTop));
    acrylic.setColorAt(0.5, QColor(9, 13, 26, int((aTop + aBot) * 0.5)));
    acrylic.setColorAt(1.0, QColor(14, 19, 36, aBot));
    p.fillRect(rect, acrylic);

    // 4. Хроматическая дисперсия цвета внутри стекла
    if (hoverMix > 0.01) {
        QLinearGradient disp(rect.topLeft(), rect.bottomRight());
        disp.setColorAt(0.0, QColor(tintColor.red(), tintColor.green(), tintColor.blue(), int(56 * hoverMix)));
        disp.setColorAt(0.5, QColor(56, 189, 248, int(28 * hoverMix)));
        disp.setColorAt(1.0, QColor(251, 191, 36, int(42 * hoverMix)));
        p.fillRect(rect, disp);
    }

    // 5. Каустический световой прожектор под курсором мыши
    if (hasCursor) {
        double spotR = std::max(rect.width(), rect.height()) * 0.72;
        QRadialGradient spot(cursorPos, spotR);
        spot.setColorAt(0.0, QColor(255, 255, 255, int(68 + 42 * hoverMix)));
        spot.setColorAt(0.35, QColor(186, 230, 253, int(30 + 24 * hoverMix)));
        spot.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(rect, spot);
    }

    // 6. Верхний зеркальный блик жидкого купола (HyperOS Specular Crown)
    QLinearGradient crown(0, rect.top(), 0, rect.top() + rect.height() * 0.46);
    crown.setColorAt(0.0, QColor(255, 255, 255, int(82 + 48 * hoverMix)));
    crown.setColorAt(0.38, QColor(255, 255, 255, int(20 + 16 * hoverMix)));
    crown.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(QRectF(rect.left(), rect.top(), rect.width(), rect.height() * 0.46), crown);

    p.restore();

    // 7. Объёмная внутренняя преломляющая кромка линзы (HyperOS 4 Refractive Lip)
    QRectF innerLip = rect.adjusted(2.0, 2.0, -2.0, -2.0);
    QLinearGradient lipGrad(innerLip.topLeft(), innerLip.bottomRight());
    lipGrad.setColorAt(0.0, QColor(255, 255, 255, int(72 + 52 * hoverMix)));
    lipGrad.setColorAt(0.5, QColor(125, 211, 252, int(24 + 36 * hoverMix)));
    lipGrad.setColorAt(1.0, QColor(251, 191, 36, int(38 + 56 * hoverMix)));
    p.setPen(QPen(QBrush(lipGrad), 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(innerLip, std::max(1.0, radius - 2.0), std::max(1.0, radius - 2.0));

    // 8. Внешний бриллиантовый кант стекла (HyperOS 4 Specular Rim)
    QLinearGradient rimGrad(rect.topLeft(), rect.bottomRight());
    rimGrad.setColorAt(0.0, QColor(255, 255, 255, int(210 + 45 * hoverMix)));
    rimGrad.setColorAt(0.35, QColor(255, 255, 255, int(75 + 70 * hoverMix)));
    rimGrad.setColorAt(0.70, QColor(56, 189, 248, int(65 + 100 * hoverMix)));
    rimGrad.setColorAt(1.0, QColor(tintColor.red(), tintColor.green(), tintColor.blue(), int(95 + 120 * hoverMix)));
    p.setPen(QPen(QBrush(rimGrad), 1.35 + 0.35 * hoverMix));
    p.drawRoundedRect(rect, radius, radius);
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
                        QColor acc = QColor(56, 189, 248),
                        bool play = false,
                        bool danger = false)
        : QPushButton(text, parent), accent(acc), isPlay(play), isDanger(danger) {
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setMinimumHeight(36);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            bool a1 = hoverSpring.step(0.016);
            bool a2 = scaleSpring.step(0.016);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void setAccentColor(const QColor &c, bool pulse = false) {
        accent = c;
        if (pulse) {
            scaleSpring.velocity += 2.2;
            hoverSpring.value = 0.85;
            if (!physTimer->isActive()) physTimer->start(16);
        }
        update();
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.026;
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
        scaleSpring.target = 0.91;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = hasMouse ? 1.026 : 1.0;
        scaleSpring.velocity += 2.8;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mouseReleaseEvent(e);
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

        QRectF r(2.0, 2.0, w - 4.0, h - 4.0);
        double rad = std::min(r.height() * 0.47, 13.0);

        if (isPlay) {
            drawHyperOs4LiquidGlass(p, this, r, rad, 0.68 + 0.32 * hp, mousePos, hasMouse, QColor(16, 185, 129), 0.28);
        } else if (isDanger) {
            drawHyperOs4LiquidGlass(p, this, r, rad, 0.58 + 0.42 * hp, mousePos, hasMouse, QColor(239, 68, 68), 0.28);
        } else {
            drawHyperOs4LiquidGlass(p, this, r, rad, hp, mousePos, hasMouse, accent, 0.21);
        }

        p.setPen(QColor(255, 255, 255));
        QFont f = font(); f.setBold(true); p.setFont(f);
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
        setFixedHeight(44);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            bool a1 = hoverSpring.step(0.016);
            bool a2 = activeSpring.step(0.016);
            bool a3 = scaleSpring.step(0.016);
            update();
            if (!a1 && !a2 && !a3) physTimer->stop();
        });
    }

    void setActiveTab(bool act) {
        isActiveTab = act;
        activeSpring.target = act ? 1.0 : 0.0;
        if (act) scaleSpring.velocity += 1.6;
        if (!physTimer->isActive()) physTimer->start(16);
    }
    void setCompactMode(bool compact) { showTitle = !compact; update(); }

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
        scaleSpring.velocity += 2.2;
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

        QRectF r(2.0, 2.0, w - 4.0, h - 4.0);
        drawHyperOs4LiquidGlass(p, this, r, 13.5, mix, mousePos, hasMouse,
                                ap > 0.2 ? QColor(251, 191, 36) : QColor(56, 189, 248),
                                0.17 + 0.12 * ap);

        if (mix > 0.04) {
            double pillH = (h - 18.0) * mix;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, int(250 * mix)));
            p.drawRoundedRect(QRectF(6.5, (h - pillH) * 0.5, 3.8, pillH), 1.9, 1.9);
        }

        QFont f = font();
        if (showTitle) {
            f.setPixelSize(13); f.setBold(isActiveTab || hp > 0.25); p.setFont(f);
            p.setPen((isActiveTab || hp > 0.2) ? QColor(255, 255, 255) : QColor(230, 238, 250));
            double sx = 16.0 + 5.0 * hp;
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
        setMouseTracking(true);
        auto *lv = new QListView(this);
        lv->setObjectName("ComboPopupList");
        setView(lv);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            bool a1 = hoverSpring.step(0.016);
            bool a2 = scaleSpring.step(0.016);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void showPopup() override {
        scaleSpring.velocity -= 1.8;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::showPopup();
        if (view() && view()->window()) {
            QWidget *popWin = view()->window();
            popWin->setAttribute(Qt::WA_TranslucentBackground);
            popWin->setStyleSheet(
                "background-color: rgba(14, 19, 36, 242); "
                "border: 1.5px solid rgba(255, 255, 255, 165); "
                "border-radius: 14px;");
            auto *fade = new QPropertyAnimation(popWin, "windowOpacity", popWin);
            fade->setDuration(160);
            fade->setStartValue(0.0);
            fade->setEndValue(1.0);
            fade->setEasingCurve(QEasingCurve::OutCubic);
            fade->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.016;
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
        scaleSpring.target = 1.0; scaleSpring.velocity += 1.8;
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

        QRectF r(2.0, 2.0, w - 4.0, h - 4.0);
        drawHyperOs4LiquidGlass(p, this, r, 12.0, hp, mousePos, hasMouse, QColor(56, 189, 248), 0.24);

        p.setPen(QColor(255, 255, 255));
        QFont f = font(); f.setBold(true); p.setFont(f);
        p.drawText(QRectF(14.0, 0, w - 46.0, h), Qt::AlignVCenter | Qt::AlignLeft, currentText());

        double badgeR = 10.5;
        QPointF badgeCenter(w - 19.5, h * 0.5);
        p.setPen(QPen(QColor(255, 255, 255, int(95 + 95 * hp)), 1.1));
        p.setBrush(QColor(255, 255, 255, int(32 + 45 * hp)));
        p.drawEllipse(badgeCenter, badgeR, badgeR);

        QPainterPath chev;
        double cx = badgeCenter.x(), cy = badgeCenter.y();
        chev.moveTo(cx - 3.0, cy - 1.2); chev.lineTo(cx, cy - 4.0); chev.lineTo(cx + 3.0, cy - 1.2);
        chev.moveTo(cx - 3.0, cy + 1.2); chev.lineTo(cx, cy + 4.0); chev.lineTo(cx + 3.0, cy + 1.2);

        p.setPen(QPen(QColor(255, 255, 255, 245), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
                if (it != surfaces.end() && it.value().hover.step(0.016)) {
                    anyActive = true;
                    k->update();
                }
            }
            const auto swKeys = switches.keys();
            for (QCheckBox *k : swKeys) {
                auto it = switches.find(k);
                if (it != switches.end()) {
                    bool s1 = it.value().pos.step(0.016);
                    bool s2 = it.value().pressStretch.step(0.016);
                    bool s3 = it.value().hover.step(0.016);
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
                connect(frame, &QObject::destroyed, this, [this, frame]() { surfaces.remove(frame); });
            } else if (event->type() == QEvent::Enter) {
                surfaces[frame].hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Leave) {
                surfaces[frame].hover.target = 0.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(frame);
                p.setRenderHint(QPainter::Antialiasing);
                auto &st = surfaces[frame];
                const QString name = frame->objectName();
                QRectF r(1.5, 1.5, frame->width() - 3.0, frame->height() - 3.0);

                QColor tint(56, 189, 248);
                double baseOpacity = 0.22;
                double rad = 19.0;
                if (name == "Sidebar") { baseOpacity = 0.28; tint = QColor(168, 85, 247); }
                else if (name == "WarningBanner") { baseOpacity = 0.32; tint = QColor(245, 158, 11); rad = 14.0; }
                else if (name == "AuthorsBox") { baseOpacity = 0.25; tint = QColor(251, 191, 36); rad = 14.0; }

                drawHyperOs4LiquidGlass(p, frame, r, rad, std::clamp(st.hover.value, 0.0, 1.0),
                                        r.center(), false, tint, baseOpacity);
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

        if (qobject_cast<QLineEdit *>(watched) || qobject_cast<QSpinBox *>(watched)) {
            auto *w = static_cast<QWidget *>(watched);
            if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
                connect(w, &QObject::destroyed, this, [this, w]() { surfaces.remove(w); });
            } else if (event->type() == QEvent::Enter) {
                surfaces[w].hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Leave) {
                surfaces[w].hover.target = 0.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(w);
                p.setRenderHint(QPainter::Antialiasing);
                auto &st = surfaces[w];
                double mix = w->hasFocus() ? 1.0 : std::clamp(st.hover.value, 0.0, 1.0);
                QRectF r(1.5, 1.5, w->width() - 3.0, w->height() - 3.0);
                drawHyperOs4LiquidGlass(p, w, r, 11.5, mix, r.center(), false,
                                        w->hasFocus() ? QColor(56, 189, 248) : QColor(255, 255, 255), 0.24);
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        QLabel *badgeLbl = nullptr;
        if (isGlassBadgeLabel(watched, &badgeLbl)) {
            if (event->type() == QEvent::Paint) {
                QPainter p(badgeLbl);
                p.setRenderHint(QPainter::Antialiasing);
                QRectF r(1.2, 1.2, badgeLbl->width() - 2.4, badgeLbl->height() - 2.4);
                const QString name = badgeLbl->objectName();
                QColor tint = (name == "RobuxCardBadge") ? QColor(16, 185, 129) : QColor(251, 191, 36);
                drawHyperOs4LiquidGlass(p, badgeLbl, r, std::min(r.height() * 0.48, 12.0),
                                        0.6, r.center(), false, tint, 0.24);
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *bar = qobject_cast<QProgressBar *>(watched)) {
            if (event->type() == QEvent::Paint) {
                QPainter p(bar);
                p.setRenderHint(QPainter::Antialiasing);
                QRectF r(1.2, 1.2, bar->width() - 2.4, bar->height() - 2.4);
                double rad = r.height() * 0.48;
                drawHyperOs4LiquidGlass(p, bar, r, rad, 0.35, r.center(), false, QColor(56, 189, 248), 0.24);

                double frac = 1.0;
                if (bar->maximum() > bar->minimum()) {
                    frac = std::clamp(double(bar->value() - bar->minimum()) / (bar->maximum() - bar->minimum()), 0.0, 1.0);
                }
                if (frac > 0.02) {
                    QRectF fillR = r.adjusted(2.0, 2.0, -2.0, -2.0);
                    fillR.setWidth(fillR.width() * frac);
                    QLinearGradient fg(fillR.topLeft(), fillR.topRight());
                    fg.setColorAt(0.0, QColor(56, 189, 248, 225));
                    fg.setColorAt(0.5, QColor(52, 211, 153, 225));
                    fg.setColorAt(1.0, QColor(251, 191, 36, 235));
                    p.setPen(QPen(QColor(255, 255, 255, 190), 0.9));
                    p.setBrush(fg);
                    p.drawRoundedRect(fillR, rad - 2.0, rad - 2.0);
                }
                p.setPen(QColor(255, 255, 255));
                QFont f = bar->font(); f.setBold(true); p.setFont(f);
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

                drawHyperOs4LiquidGlass(p, chk, trackRect, swH * 0.5, std::max(tPos, hov),
                                        trackRect.center(), false,
                                        tPos > 0.3 ? QColor(16, 185, 129) : QColor(56, 189, 248),
                                        0.20 + 0.24 * tPos);

                if (tPos > 0.05) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(16, 185, 129, int(190 * tPos)));
                    p.drawRoundedRect(trackRect.adjusted(1.0, 1.0, -1.0, -1.0), swH * 0.46, swH * 0.46);
                }

                double thumbBaseD = swH - 4.4;
                double thumbW = thumbBaseD + stretch * 7.0;
                double minX = trackRect.left() + 2.2;
                double maxX = trackRect.right() - 2.2 - thumbW;
                double thumbX = minX + (maxX - minX) * tPos;
                QRectF thumbRect(thumbX, swY + 2.2, thumbW, thumbBaseD);

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 105));
                p.drawRoundedRect(thumbRect.translated(0, 1.4), thumbBaseD * 0.5, thumbBaseD * 0.5);

                QLinearGradient thumbGrad(thumbRect.topLeft(), thumbRect.bottomRight());
                thumbGrad.setColorAt(0.0, QColor(255, 255, 255, 255));
                thumbGrad.setColorAt(1.0, QColor(226, 232, 240, 245));
                p.setBrush(thumbGrad);
                p.setPen(QPen(QColor(255, 255, 255, 255), 1.1));
                p.drawRoundedRect(thumbRect, thumbBaseD * 0.5, thumbBaseD * 0.5);

                p.setPen(hov > 0.2 ? QColor(255, 255, 255) : QColor(244, 248, 255));
                QFont f = chk->font(); f.setWeight(QFont::DemiBold); p.setFont(f);
                p.drawText(QRectF(trackRect.right() + 12.0, 0, chk->width() - trackRect.right() - 14.0, h),
                           Qt::AlignVCenter | Qt::AlignLeft, chk->text());
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
