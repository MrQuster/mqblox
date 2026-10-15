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
#include <QPushButton>
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
    double stiffness = 340.0;
    double damping = 25.0;

    bool step(double dt = 0.016) {
        double force = (target - value) * stiffness - velocity * damping;
        velocity += force * dt;
        value += velocity * dt;
        if (std::abs(target - value) < 0.003 && std::abs(velocity) < 0.015) {
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

inline void drawCleanHyperOsGlass(QPainter &p,
                                  const QWidget *widget,
                                  const QRectF &rect,
                                  double radius,
                                  double hoverMix,
                                  const QColor &tintColor = QColor(56, 189, 248),
                                  double darkTintAlpha = 0.36) {
    if (rect.width() <= 4 || rect.height() <= 4) return;

    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);

    p.save();
    p.setClipPath(path);
    if (Ton618GlWidget *bg = findBackdropTon618(widget)) {
        const QPixmap &frosted = bg->frostedBackdrop();
        if (!frosted.isNull()) {
            QPoint posInBg = widget->mapTo(bg, rect.topLeft().toPoint());
            QRect srcRect(posInBg, rect.size().toSize());
            p.drawPixmap(rect.topLeft(), frosted, srcRect);
        }
    }
    p.restore();

    QLinearGradient glassFill(rect.topLeft(), rect.bottomRight());
    int aTop = std::clamp(int((darkTintAlpha + 0.05 * hoverMix) * 255), 35, 225);
    int aBot = std::clamp(int((darkTintAlpha + 0.12 + 0.05 * hoverMix) * 255), 50, 235);
    glassFill.setColorAt(0.0, QColor(20, 27, 48, aTop));
    glassFill.setColorAt(0.5, QColor(10, 14, 26, int((aTop + aBot) * 0.5)));
    glassFill.setColorAt(1.0, QColor(14, 19, 36, aBot));

    p.setPen(Qt::NoPen);
    p.setBrush(glassFill);
    p.drawRoundedRect(rect, radius, radius);

    if (hoverMix > 0.01) {
        QLinearGradient glow(rect.topLeft(), rect.bottomRight());
        glow.setColorAt(0.0, QColor(tintColor.red(), tintColor.green(), tintColor.blue(), int(46 * hoverMix)));
        glow.setColorAt(1.0, QColor(251, 191, 36, int(28 * hoverMix)));
        p.setBrush(glow);
        p.drawRoundedRect(rect, radius, radius);
    }

    QLinearGradient sheen(0, rect.top(), 0, rect.top() + rect.height() * 0.45);
    sheen.setColorAt(0.0, QColor(255, 255, 255, int(48 + 36 * hoverMix)));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setBrush(sheen);
    p.drawRoundedRect(rect.adjusted(1.0, 1.0, -1.0, -rect.height() * 0.52),
                      std::max(1.0, radius - 1.0), std::max(1.0, radius - 1.0));

    QLinearGradient rim(rect.topLeft(), rect.bottomRight());
    rim.setColorAt(0.0, QColor(255, 255, 255, int(155 + 75 * hoverMix)));
    rim.setColorAt(0.5, QColor(148, 163, 184, int(55 + 65 * hoverMix)));
    rim.setColorAt(1.0, QColor(tintColor.red(), tintColor.green(), tintColor.blue(), int(70 + 115 * hoverMix)));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(rim), 1.2 + 0.3 * hoverMix));
    p.drawRoundedRect(rect, radius, radius);
}

class AnimButton : public QPushButton {
    Q_OBJECT
public:
    IosSpring hoverSpring{0.0, 0.0, 0.0, 320.0, 24.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 400.0, 22.0};
    QColor accent;
    bool isPlay = false, isDanger = false;
    bool hasMouse = false;
    QTimer *physTimer = nullptr;

    explicit AnimButton(const QString &text,
                        QWidget *parent = nullptr,
                        QColor acc = QColor(56, 189, 248),
                        bool play = false,
                        bool danger = false)
        : QPushButton(text, parent), accent(acc), isPlay(play), isDanger(danger) {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(36);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            bool a1 = hoverSpring.step(0.016);
            bool a2 = scaleSpring.step(0.016);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true;
        hoverSpring.target = 1.0; scaleSpring.target = 1.018;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false;
        hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.94;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = hasMouse ? 1.018 : 1.0;
        scaleSpring.velocity += 2.0;
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
        double rad = std::min(r.height() * 0.46, 12.0);

        if (isPlay) {
            drawCleanHyperOsGlass(p, this, r, rad, 0.65 + 0.35 * hp, QColor(16, 185, 129), 0.35);
        } else if (isDanger) {
            drawCleanHyperOsGlass(p, this, r, rad, 0.55 + 0.45 * hp, QColor(239, 68, 68), 0.35);
        } else {
            drawCleanHyperOsGlass(p, this, r, rad, hp, accent, 0.32);
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
    IosSpring hoverSpring{0.0, 0.0, 0.0, 300.0, 23.0};
    IosSpring activeSpring{0.0, 0.0, 0.0, 280.0, 22.0};
    QTimer *physTimer = nullptr;

    AnimNavButton(const QString &ico, const QString &title, QWidget *parent = nullptr)
        : QPushButton(parent), iconStr(ico), titleStr(title) {
        setFixedHeight(44);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            bool a1 = hoverSpring.step(0.016);
            bool a2 = activeSpring.step(0.016);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void setActiveTab(bool act) {
        isActiveTab = act;
        activeSpring.target = act ? 1.0 : 0.0;
        if (!physTimer->isActive()) physTimer->start(16);
    }
    void setCompactMode(bool compact) { showTitle = !compact; update(); }

protected:
    void enterEvent(QEnterEvent *e) override {
        hoverSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hoverSpring.target = 0.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::leaveEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double ap = std::clamp(activeSpring.value, 0.0, 1.0);
        double mix = std::max(hp * 0.75, ap);

        QRectF r(2.0, 2.0, w - 4.0, h - 4.0);
        drawCleanHyperOsGlass(p, this, r, 13.0, mix,
                              ap > 0.2 ? QColor(251, 191, 36) : QColor(56, 189, 248),
                              0.26 + 0.12 * ap);

        if (mix > 0.04) {
            double pillH = (h - 18.0) * mix;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, int(245 * mix)));
            p.drawRoundedRect(QRectF(6.5, (h - pillH) * 0.5, 3.6, pillH), 1.8, 1.8);
        }

        QFont f = font();
        if (showTitle) {
            f.setPixelSize(13); f.setBold(isActiveTab || hp > 0.25); p.setFont(f);
            p.setPen((isActiveTab || hp > 0.2) ? QColor(255, 255, 255) : QColor(226, 232, 245));
            double sx = 16.0 + 4.0 * hp;
            p.drawText(QRectF(sx, 0, w - sx - 6.0, h), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1   %2").arg(iconStr, titleStr));
        } else {
            f.setPixelSize(17); p.setFont(f); p.setPen(QColor(255, 255, 255));
            p.drawText(r, Qt::AlignCenter, iconStr);
        }
    }
};

class DarkComboBox : public QComboBox {
    Q_OBJECT
public:
    IosSpring hoverSpring{0.0, 0.0, 0.0, 310.0, 24.0};
    QTimer *physTimer = nullptr;

    explicit DarkComboBox(QWidget *parent = nullptr) : QComboBox(parent) {
        setMinimumHeight(36);
        auto *lv = new QListView(this);
        lv->setObjectName("ComboPopupList");
        setView(lv);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            if (!hoverSpring.step(0.016)) physTimer->stop();
            update();
        });
    }

    void showPopup() override {
        QComboBox::showPopup();
        if (view() && view()->window()) {
            QWidget *popWin = view()->window();
            popWin->setAttribute(Qt::WA_TranslucentBackground);
            popWin->setStyleSheet(
                "background-color: rgba(14, 19, 36, 245); "
                "border: 1.5px solid rgba(255, 255, 255, 155); "
                "border-radius: 14px;");
        }
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hoverSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hoverSpring.target = 0.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::leaveEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);

        QRectF r(2.0, 2.0, w - 4.0, h - 4.0);
        drawCleanHyperOsGlass(p, this, r, 11.5, hp, QColor(56, 189, 248), 0.34);

        p.setPen(QColor(255, 255, 255));
        QFont f = font(); f.setBold(true); p.setFont(f);
        p.drawText(QRectF(14.0, 0, w - 46.0, h), Qt::AlignVCenter | Qt::AlignLeft, currentText());

        double badgeR = 10.5;
        QPointF badgeCenter(w - 19.5, h * 0.5);
        p.setPen(QPen(QColor(255, 255, 255, int(90 + 90 * hp)), 1.0));
        p.setBrush(QColor(255, 255, 255, int(28 + 40 * hp)));
        p.drawEllipse(badgeCenter, badgeR, badgeR);

        QPainterPath chev;
        double cx = badgeCenter.x(), cy = badgeCenter.y();
        chev.moveTo(cx - 3.0, cy - 1.2); chev.lineTo(cx, cy - 4.0); chev.lineTo(cx + 3.0, cy - 1.2);
        chev.moveTo(cx - 3.0, cy + 1.2); chev.lineTo(cx, cy + 4.0); chev.lineTo(cx + 3.0, cy + 1.2);

        p.setPen(QPen(QColor(255, 255, 255, 240), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(chev);
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
    struct SwitchState {
        IosSpring pos{0.0, 0.0, 0.0, 330.0, 23.0};
        IosSpring pressStretch{0.0, 0.0, 0.0, 390.0, 24.0};
        IosSpring hover{0.0, 0.0, 0.0, 290.0, 24.0};
        bool initialized = false;
    };

    QHash<QCheckBox *, SwitchState> switches;
    QTimer *tickTimer = nullptr;

    explicit LiquidGlassEngine(QObject *parent = nullptr) : QObject(parent) {
        tickTimer = new QTimer(this);
        connect(tickTimer, &QTimer::timeout, this, [this]() {
            bool anyActive = false;
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
            if (event->type() == QEvent::Paint) {
                QPainter p(frame);
                p.setRenderHint(QPainter::Antialiasing);
                const QString name = frame->objectName();
                QRectF r(1.5, 1.5, frame->width() - 3.0, frame->height() - 3.0);

                QColor tint(56, 189, 248);
                double baseOpacity = 0.36;
                double rad = 18.0;
                if (name == "Sidebar") { baseOpacity = 0.44; tint = QColor(168, 85, 247); }
                else if (name == "WarningBanner") { baseOpacity = 0.42; tint = QColor(245, 158, 11); rad = 14.0; }
                else if (name == "AuthorsBox") { baseOpacity = 0.38; tint = QColor(251, 191, 36); rad = 14.0; }

                drawCleanHyperOsGlass(p, frame, r, rad, 0.15, tint, baseOpacity);
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

        if (qobject_cast<QLineEdit *>(watched) || qobject_cast<QSpinBox *>(watched)) {
            auto *w = static_cast<QWidget *>(watched);
            if (event->type() == QEvent::Paint) {
                QPainter p(w);
                p.setRenderHint(QPainter::Antialiasing);
                double mix = w->hasFocus() ? 0.9 : 0.15;
                QRectF r(1.5, 1.5, w->width() - 3.0, w->height() - 3.0);
                drawCleanHyperOsGlass(p, w, r, 11.5, mix,
                                      w->hasFocus() ? QColor(56, 189, 248) : QColor(255, 255, 255), 0.35);
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
                drawCleanHyperOsGlass(p, badgeLbl, r, std::min(r.height() * 0.48, 11.0), 0.5, tint, 0.36);
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
                drawCleanHyperOsGlass(p, bar, r, rad, 0.25, QColor(56, 189, 248), 0.36);

                double frac = 1.0;
                if (bar->maximum() > bar->minimum()) {
                    frac = std::clamp(double(bar->value() - bar->minimum()) / (bar->maximum() - bar->minimum()), 0.0, 1.0);
                }
                if (frac > 0.02) {
                    QRectF fillR = r.adjusted(2.0, 2.0, -2.0, -2.0);
                    fillR.setWidth(fillR.width() * frac);
                    QLinearGradient fg(fillR.topLeft(), fillR.topRight());
                    fg.setColorAt(0.0, QColor(56, 189, 248, 220));
                    fg.setColorAt(0.5, QColor(52, 211, 153, 220));
                    fg.setColorAt(1.0, QColor(251, 191, 36, 230));
                    p.setPen(QPen(QColor(255, 255, 255, 180), 0.9));
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
                    s.pos.velocity += c ? 2.2 : -2.2;
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

                drawCleanHyperOsGlass(p, chk, trackRect, swH * 0.5, std::max(tPos, hov),
                                      tPos > 0.3 ? QColor(16, 185, 129) : QColor(56, 189, 248), 0.35);

                if (tPos > 0.05) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(16, 185, 129, int(190 * tPos)));
                    p.drawRoundedRect(trackRect.adjusted(1.0, 1.0, -1.0, -1.0), swH * 0.46, swH * 0.46);
                }

                double thumbBaseD = swH - 4.4;
                double thumbW = thumbBaseD + stretch * 6.5;
                double minX = trackRect.left() + 2.2;
                double maxX = trackRect.right() - 2.2 - thumbW;
                double thumbX = minX + (maxX - minX) * tPos;
                QRectF thumbRect(thumbX, swY + 2.2, thumbW, thumbBaseD);

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 100));
                p.drawRoundedRect(thumbRect.translated(0, 1.4), thumbBaseD * 0.5, thumbBaseD * 0.5);

                QLinearGradient thumbGrad(thumbRect.topLeft(), thumbRect.bottomRight());
                thumbGrad.setColorAt(0.0, QColor(255, 255, 255, 255));
                thumbGrad.setColorAt(1.0, QColor(226, 232, 240, 245));
                p.setBrush(thumbGrad);
                p.setPen(QPen(QColor(255, 255, 255, 255), 1.0));
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
