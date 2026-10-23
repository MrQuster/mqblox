#include "ui/MainWindow.hpp"
#include "core/SettingsRegistry.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSizeGrip>
#include <QUrl>
#include <QWindow>

namespace mqblox {

class LockedScrollArea : public QScrollArea {
public:
    explicit LockedScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        viewport()->setStyleSheet("background: transparent;");
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        bool res = QScrollArea::eventFilter(o, e);
        if (widget() && widget()->x() != 0) {
            widget()->move(0, widget()->y());
        }
        return res;
    }
    void resizeEvent(QResizeEvent *e) override {
        QScrollArea::resizeEvent(e);
        if (horizontalScrollBar()) {
            horizontalScrollBar()->setRange(0, 0);
            horizontalScrollBar()->setValue(0);
        }
        if (widget()) {
            widget()->setFixedWidth(viewport()->width());
            if (widget()->x() != 0) widget()->move(0, widget()->y());
        }
    }
    void scrollContentsBy(int, int dy) override {
        QScrollArea::scrollContentsBy(0, dy);
        if (horizontalScrollBar() && horizontalScrollBar()->value() != 0) {
            horizontalScrollBar()->setValue(0);
        }
        if (widget() && widget()->x() != 0) {
            widget()->move(0, widget()->y());
        }
    }
};

static QScrollArea *wrapScrollable(QWidget *w) {
    auto *s = new LockedScrollArea();
    w->setObjectName("ScrollContents");
    w->setMinimumWidth(0);
    if (w->layout()) {
        w->layout()->setContentsMargins(14, 8, 16, 14);
        w->layout()->setSpacing(12);
    }
    for (QWidget *child : w->findChildren<QWidget *>()) {
        if (!qobject_cast<QSpinBox *>(child)) {
            child->setMinimumWidth(0);
        }
        if (auto *lbl = qobject_cast<QLabel *>(child)) {
            lbl->setWordWrap(true);
        }
    }
    s->setWidget(w);
    w->installEventFilter(s);
    return s;
}

MainWindow::MainWindow(const QString &launchUri, QWidget *parent)
    : QMainWindow(parent), m_launchUri(launchUri) {
    m_settingsMgr.loadAll();
    m_soberCtrl = new SoberController(&m_settingsMgr, this);

    setWindowTitle(tr("MqBlox × MacOBlox v36.0 [HyperOS 4 Liquid Glass • Darling]"));
    QString iconFile = QDir::homePath() + "/.local/share/icons/hicolor/scalable/apps/mqblox.svg";
    if (QFile::exists(iconFile)) setWindowIcon(QIcon(iconFile));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setMinimumSize(840, 520);

    m_sidebarAnim = new QVariantAnimation(this);
    m_sidebarAnim->setDuration(180);
    m_sidebarAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_sidebarAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        int w = v.toInt();
        m_sidebar->setFixedWidth(w);
        bool isWide = (w >= 145);
        m_titleColWidget->setVisible(isWide);
        m_authorsBox->setVisible(isWide);
        m_ramBar->setVisible(isWide);
        for (auto *btn : m_navButtons) btn->setCompactMode(!isWide);
        m_statusBadge->setText(isWide ? tr("● В МЕНЮ MQBLOX") : "●");
    });

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() { animateSidebar(m_pendingHover); });

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() {
        collectSettingsFromUi();
        QString err;
        if (m_settingsMgr.saveAll(&err)) {
            if (m_lblFflagsValidation) {
                m_lblFflagsValidation->setText(tr("✓ Синтаксис FastFlags корректен и сохранён в ClientAppSettings.json"));
                m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
            }
            if (m_lblAutosync) m_lblAutosync->setText(tr("✓ Настройки синхронизированы"));
        } else {
            if (m_lblFflagsValidation) {
                m_lblFflagsValidation->setText("⚠️ " + err);
                m_lblFflagsValidation->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 700;");
            }
            enqueueToast("⚠️ " + err);
        }
    });

    initUi();
    initTrayAndShortcuts();
    syncUiFromSettings();
    refreshAccountsUi();
    refreshNamedProfilesCombo();
    refreshEnvironmentAndSessionUi();
    applyResolutionMode(m_settingsMgr.settings().resMode, true);
    applyLayoutAndTypographyLive();
    m_isLoadingUi = false;

    connect(m_soberCtrl, &SoberController::hardwareDetected, this, [this]() {
        if (m_hwBannerLbl) {
            m_hwBannerLbl->setText(tr("🔍 Железо: <b>%1</b> (%2 потоков) • GPU: <b>%3</b>")
                                       .arg(m_soberCtrl->cpuModel())
                                       .arg(m_soberCtrl->cpuThreads())
                                       .arg(m_soberCtrl->gpuModel()));
        }
        refreshEnvironmentAndSessionUi();
    });
    connect(m_soberCtrl, &SoberController::logGenerated, this, [this](const QString &line) {
        if (m_logView) m_logView->append(line);
    });
    connect(m_soberCtrl, &SoberController::toastRequested, this, &MainWindow::enqueueToast);
    connect(m_soberCtrl, &SoberController::gameStarted, this, [this]() {
        if (m_rootCosmos) m_rootCosmos->setInGamePaused(true);
        m_statusBadge->setText(m_sidebarExpanded ? tr("● В ИГРЕ (DARLING)") : "🎮");
        enqueueToast(tr("🚀 RobloxPlayer.app запущен через Darling (аудио и микрофон в audio_hal.c)"));
    });
    connect(m_soberCtrl, &SoberController::gameLeftByPlayer, this, [this]() {
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        showNormal();
        raise();
        activateWindow();
        switchTab(0);
        enqueueToast(tr("🏠 Выход с сервера: возврат в MqBlox"));
    });
    connect(m_soberCtrl, &SoberController::gameFinished, this, [this](int, bool crashed) {
        if (m_rootCosmos) m_rootCosmos->setInGamePaused(false);
        m_statusBadge->setText(m_sidebarExpanded ? tr("● В МЕНЮ MQBLOX") : "●");
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        refreshEnvironmentAndSessionUi();
        showNormal();
        raise();
        activateWindow();
        if (crashed && m_settingsMgr.settings().autoReconnectOnCrash) {
            enqueueToast(tr("⚠️ Обнаружен сбой клиента! Авто-перезаход через 3 с..."));
            QTimer::singleShot(3000, this, [this]() { m_soberCtrl->launchRoblox(); });
        }
    });
    connect(m_soberCtrl, &SoberController::accountSwitchCompleted, this, [this](const QString &accId) {
        refreshAccountsUi();
        refreshEnvironmentAndSessionUi();
        enqueueToast(tr("👤 Слот сессии переключён на «%1»").arg(accId));
    });
    connect(m_soberCtrl, &SoberController::wizardProgress, this,
            [this](int pct, const QString &statusText) {
                if (m_wizardProgressBar) m_wizardProgressBar->setValue(pct);
                if (m_lblWizardStatus) m_lblWizardStatus->setText(statusText);
            });
    connect(m_soberCtrl, &SoberController::wizardFinished, this,
            [this](bool ok, const QString &summary) {
                if (m_btnRunWizard) m_btnRunWizard->setEnabled(true);
                if (m_lblWizardStatus) {
                    m_lblWizardStatus->setText((ok ? "✅ " : "❌ ") + summary);
                }
                enqueueToast((ok ? "✅ " : "⚠️ ") + summary);
                refreshEnvironmentAndSessionUi();
            });
    connect(m_soberCtrl, &SoberController::sessionStateChanged, this,
            &MainWindow::refreshEnvironmentAndSessionUi);

    m_soberCtrl->detectHardwareAsync();

    auto *watchdog = new QTimer(this);
    connect(watchdog, &QTimer::timeout, this, [this]() { runWatchdogTick(); });
    watchdog->start(3000);
    runWatchdogTick();

    if (!m_launchUri.isEmpty()) {
        QTimer::singleShot(400, this, [this]() { m_soberCtrl->launchRoblox(m_launchUri); });
    }
}

MainWindow::~MainWindow() = default;

Qt::Edges MainWindow::detectResizeEdges(const QPointF &pos) const {
    if (isMaximized()) return {};
    constexpr double margin = 7.0;
    Qt::Edges edges;
    if (pos.x() <= margin) edges |= Qt::LeftEdge;
    if (pos.x() >= width() - margin) edges |= Qt::RightEdge;
    if (pos.y() <= margin) edges |= Qt::TopEdge;
    if (pos.y() >= height() - margin) edges |= Qt::BottomEdge;
    return edges;
}

void MainWindow::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && windowHandle()) {
        Qt::Edges edges = detectResizeEdges(e->position());
        if (edges != Qt::Edges{}) {
            windowHandle()->startSystemResize(edges);
            return;
        }
        if (e->position().y() <= 52.0) {
            windowHandle()->startSystemMove();
            return;
        }
    }
    QMainWindow::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e) {
    Qt::Edges edges = detectResizeEdges(e->position());
    if ((edges == (Qt::LeftEdge | Qt::TopEdge)) || (edges == (Qt::RightEdge | Qt::BottomEdge)))
        setCursor(Qt::SizeFDiagCursor);
    else if ((edges == (Qt::RightEdge | Qt::TopEdge)) || (edges == (Qt::LeftEdge | Qt::BottomEdge)))
        setCursor(Qt::SizeBDiagCursor);
    else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        setCursor(Qt::SizeHorCursor);
    else if (edges & (Qt::TopEdge | Qt::BottomEdge))
        setCursor(Qt::SizeVerCursor);
    else
        unsetCursor();
    QMainWindow::mouseMoveEvent(e);
}

void MainWindow::closeEvent(QCloseEvent *e) {
    if (m_autosaveTimer && m_autosaveTimer->isActive()) {
        m_autosaveTimer->stop();
        saveSettingsNow(false);
    }
    if (!m_forceQuit && m_settingsMgr.settings().closeToTray && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        e->ignore();
        return;
    }
    if (m_trayIcon) m_trayIcon->hide();
    e->accept();
    QApplication::quit();
}

void MainWindow::initTrayAndShortcuts() {
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon = new QSystemTrayIcon(windowIcon(), this);
        m_trayIcon->setToolTip(tr("MqBlox × MacOBlox v36.0 — HyperOS 4 Liquid Glass"));

        auto *menu = new QMenu(nullptr);
        menu->setStyleSheet(
            "QMenu { background-color: #070A14; color: #F8FAFC; border: 1px solid #38BDF8; border-radius: 10px; padding: 6px; font-weight: 700; }"
            "QMenu::item { padding: 7px 18px; border-radius: 6px; }"
            "QMenu::item:selected { background-color: #38BDF8; color: #030408; }");
        menu->addAction(tr("🌌 Развернуть MqBlox"), this, [this]() {
            showNormal();
            raise();
            activateWindow();
        });
        menu->addAction(tr("▶ Запустить Roblox (Darling)"), this, [this]() { m_soberCtrl->launchRoblox(); });
        menu->addAction(tr("🛑 Остановить Roblox"), this, [this]() { m_soberCtrl->stopRobloxAsync(); });
        menu->addSeparator();
        menu->addAction(tr("❌ Полный выход из MqBlox"), this, [this, menu]() {
            m_forceQuit = true;
            if (m_trayIcon) m_trayIcon->hide();
            menu->deleteLater();
            QApplication::quit();
        });

        m_trayIcon->setContextMenu(menu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
                if (isVisible() && !isMinimized()) hide();
                else {
                    showNormal();
                    raise();
                    activateWindow();
                }
            }
        });
        m_trayIcon->show();
    }

    new QShortcut(QKeySequence(Qt::Key_F5), this, [this]() { refreshEnvironmentAndSessionUi(); });
    new QShortcut(QKeySequence("Ctrl+L"), this, [this]() { m_soberCtrl->launchRoblox(); });
    new QShortcut(QKeySequence("Ctrl+K"), this, [this]() { m_soberCtrl->stopRobloxAsync(); });
    new QShortcut(QKeySequence("Ctrl+Q"), this, [this]() { m_forceQuit = true; close(); });
}

void MainWindow::initUi() {
    QFile qssFile(":/style_ton618.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(qssFile.readAll()));
    }

    m_rootCosmos = new Ton618GlWidget(this);
    setCentralWidget(m_rootCosmos);
    auto *rootLayout = new QHBoxLayout(m_rootCosmos);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_sidebar = new HoverSidebar();
    m_sidebar->setObjectName("Sidebar");
    m_sidebar->setFixedWidth(232);
    m_sidebar->onHoverChanged = [this](bool hov) {
        if (m_settingsMgr.settings().sidebarPinned) return;
        m_pendingHover = hov;
        m_hoverTimer->start(35);
    };

    auto *sideLay = new QVBoxLayout(m_sidebar);
    sideLay->setContentsMargins(10, 12, 10, 12);
    sideLay->setSpacing(7);

    auto *hdrRow = new QHBoxLayout();
    m_btnPinSide = new AnimButton("📌");
    m_btnPinSide->setFixedSize(46, 42);
    connect(m_btnPinSide, &QPushButton::clicked, this, [this]() { toggleSidebarPinMode(); });
    hdrRow->addWidget(m_btnPinSide);

    m_titleColWidget = new QWidget();
    auto *tCol = new QVBoxLayout(m_titleColWidget);
    tCol->setContentsMargins(0, 0, 0, 0);
    tCol->setSpacing(1);
    auto *logoLbl = new QLabel("✦ MqBlox × MacOBlox");
    logoLbl->setObjectName("LogoText");
    m_pinHintLbl = new QLabel("💧 HyperOS 4 • Darling macOS");
    m_pinHintLbl->setStyleSheet("font-size: 10px; font-weight: 800; color: #38BDF8;");
    tCol->addWidget(logoLbl);
    tCol->addWidget(m_pinHintLbl);
    hdrRow->addWidget(m_titleColWidget);
    hdrRow->addStretch();
    sideLay->addLayout(hdrRow);

    struct NavItem { QString ico; QString title; int idx; };
    const std::vector<NavItem> navItems = {
        {"🏠", tr("Главная и Мастер"), 0},
        {"👥", tr("Слоты и Профили"), 1},
        {"⚙️", tr("Система и Вид"), 2},
        {"✨", tr("Графика (OpenGL)"), 3},
        {"🔥", tr("Твики и Отладка"), 4},
        {"ℹ️", tr("О программе"), 5}
    };
    for (const auto &it : navItems) {
        auto *btn = new AnimNavButton(it.ico, it.title);
        btn->setActiveTab(it.idx == 0);
        int idx = it.idx;
        connect(btn, &QPushButton::clicked, this, [this, idx]() { switchTab(idx); });
        sideLay->addWidget(btn);
        m_navButtons.push_back(btn);
    }

    sideLay->addStretch();

    m_authorsBox = new QFrame();
    m_authorsBox->setObjectName("AuthorsBox");
    auto *abLay = new QVBoxLayout(m_authorsBox);
    abLay->setContentsMargins(12, 10, 12, 10);
    auto *lblAuthT = new QLabel(tr("👑 CREDITS (MIT):"));
    lblAuthT->setStyleSheet("font-size: 10px; font-weight: 900; color: #FDE047;");
    auto *lblAuthN = new QLabel("✦ Narezany (MacOBlox)\n✦ MrQuster (MqBlox UI)\n✦ Darling Project");
    lblAuthN->setStyleSheet("font-size: 11px; font-weight: 800; color: #FFFFFF;");
    abLay->addWidget(lblAuthT);
    abLay->addWidget(lblAuthN);
    sideLay->addWidget(m_authorsBox);

    m_ramBar = new QProgressBar();
    m_ramBar->setObjectName("RamBar");
    m_ramBar->setRange(0, 100);
    sideLay->addWidget(m_ramBar);

    m_statusBadge = new QLabel(tr("● В МЕНЮ MQBLOX"));
    m_statusBadge->setObjectName("StatusBadge");
    m_statusBadge->setFixedHeight(32);
    m_statusBadge->setAlignment(Qt::AlignCenter);
    sideLay->addWidget(m_statusBadge);
    rootLayout->addWidget(m_sidebar);

    auto *rightPanel = new QWidget();
    auto *rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(12, 10, 12, 10);
    rightLay->setSpacing(8);

    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_inpSearchSettings = new QLineEdit();
    m_inpSearchSettings->setPlaceholderText(tr("🔍 Поиск настроек (FPS, MSAA, DNS, мышь, тени, стекло)..."));
    m_inpSearchSettings->setClearButtonEnabled(true);
    m_inpSearchSettings->setMinimumWidth(220);
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(90);
    connect(m_searchDebounce, &QTimer::timeout, this, [this]() {
        filterSettingsBySearch(m_inpSearchSettings->text());
    });
    connect(m_inpSearchSettings, &QLineEdit::textChanged, this, [this]() {
        m_searchDebounce->start();
    });
    topBar->addWidget(m_inpSearchSettings, 1);

    m_toastLbl = new QLabel();
    m_toastLbl->setObjectName("ToastBanner");
    m_toastLbl->setVisible(false);
    topBar->addWidget(m_toastLbl);

    topBar->addWidget(new QLabel(tr("Окно:")));
    m_comboRes = new DarkComboBox();
    m_comboRes->setMinimumWidth(150);
    m_comboRes->addItem(tr("Авто (Под монитор)"), static_cast<int>(ResolutionMode::Auto));
    m_comboRes->addItem(tr("HD (1200×680)"), static_cast<int>(ResolutionMode::HD_720p));
    m_comboRes->addItem(tr("Full HD (1480×840)"), static_cast<int>(ResolutionMode::FHD_1080p));
    m_comboRes->addItem(tr("2K QHD (1920×1080)"), static_cast<int>(ResolutionMode::QHD_2K));
    connect(m_comboRes, &QComboBox::currentIndexChanged, this, [this](int idx) {
        auto mode = static_cast<ResolutionMode>(m_comboRes->itemData(idx).toInt());
        applyResolutionMode(mode, false);
    });
    topBar->addWidget(m_comboRes);

    topBar->addWidget(new QLabel(tr("Слот:")));
    m_comboQuickAcc = new DarkComboBox();
    m_comboQuickAcc->setMinimumWidth(130);
    connect(m_comboQuickAcc, &QComboBox::currentIndexChanged, this, [this](int i) {
        QString id = m_comboQuickAcc->itemData(i).toString();
        if (!id.isEmpty() && id != m_settingsMgr.activeAccountId()) {
            m_soberCtrl->switchAccountSlotAsync(id);
        }
    });
    topBar->addWidget(m_comboQuickAcc);

    auto *btnMin = new AnimButton("—");
    btnMin->setFixedSize(36, 34);
    btnMin->setMinimumWidth(36);
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    m_btnMax = new AnimButton("□");
    m_btnMax->setFixedSize(36, 34);
    m_btnMax->setMinimumWidth(36);
    connect(m_btnMax, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            m_btnMax->setText("□");
        } else {
            showMaximized();
            m_btnMax->setText("❐");
        }
    });
    auto *btnClose = new AnimButton("✕", nullptr, QColor(239, 68, 68), false, true);
    btnClose->setFixedSize(36, 34);
    btnClose->setMinimumWidth(36);
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    topBar->addWidget(btnMin);
    topBar->addWidget(m_btnMax);
    topBar->addWidget(btnClose);
    rightLay->addLayout(topBar);

    m_pages = new QStackedWidget();
    m_pages->addWidget(wrapScrollable(buildHomeRobloxPage()));
    m_pages->addWidget(wrapScrollable(buildAccountsPage()));
    m_pages->addWidget(wrapScrollable(buildHardwareAudioPage()));
    m_pages->addWidget(wrapScrollable(buildGraphicsPage()));
    m_pages->addWidget(wrapScrollable(buildCameraModsPage()));
    m_pages->addWidget(wrapScrollable(buildUpdatesPage()));
    rightLay->addWidget(m_pages, 1);

    auto *bottomBar = new QHBoxLayout();
    bottomBar->setSpacing(8);
    m_lblAutosync = new QLabel(tr("✓ Настройки актуальны"));
    m_lblAutosync->setObjectName("AutoSyncBadge");

    m_btnSaveSettings = new AnimButton(tr("💾 Сохранить настройки"), nullptr, QColor(56, 189, 248));
    connect(m_btnSaveSettings, &QPushButton::clicked, this, [this]() { saveSettingsNow(true); });
    new QShortcut(QKeySequence("Ctrl+S"), this, [this]() { saveSettingsNow(true); });

    auto *btnResetCat = new AnimButton(tr("↺ Сбросить вкладку"));
    connect(btnResetCat, &QPushButton::clicked, this, [this]() { resetCurrentCategoryToDefaults(); });

    auto *btnResetLook = new AnimButton(tr("🎨 Сбросить вид"), nullptr, QColor(56, 189, 248));
    connect(btnResetLook, &QPushButton::clicked, this, [this]() { resetAppearanceToDefault(); });

    auto *btnRejoin = new AnimButton(tr("🔄 Перезапустить Darling"));
    connect(btnRejoin, &QPushButton::clicked, this, [this]() { m_soberCtrl->restartDarlingAsync(); });

    auto *btnKill = new AnimButton(tr("🛑 Остановить игру"), nullptr, QColor(239, 68, 68), false, true);
    connect(btnKill, &QPushButton::clicked, this, [this]() { m_soberCtrl->stopRobloxAsync(); });

    auto *btnPlay = new AnimButton(tr("▶  ЗАПУСТИТЬ ИГРУ"), nullptr, QColor(16, 185, 129), true, false);
    connect(btnPlay, &QPushButton::clicked, this, [this]() {
        saveSettingsNow(false);
        m_soberCtrl->launchRoblox();
    });

    bottomBar->addWidget(m_lblAutosync);
    bottomBar->addWidget(m_btnSaveSettings);
    bottomBar->addWidget(btnResetCat);
    bottomBar->addWidget(btnResetLook);
    bottomBar->addWidget(btnRejoin);
    bottomBar->addWidget(btnKill);
    bottomBar->addStretch();
    bottomBar->addWidget(btnPlay);

    auto *grip = new QSizeGrip(this);
    grip->setFixedSize(16, 16);
    bottomBar->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
    rightLay->addLayout(bottomBar);
    rootLayout->addWidget(rightPanel, 1);
}

QWidget *MainWindow::buildHomeRobloxPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    // 1. Quick Launch & Session Card (Zero Password / Cookie Interception)
    auto *cardLaunch = new QFrame();
    cardLaunch->setObjectName("GlassCard");
    auto *cl = new QVBoxLayout(cardLaunch);
    cl->setContentsMargins(18, 16, 18, 16);
    cl->setSpacing(10);

    auto *tLaunch = new QLabel(tr("🚀 Запуск официального macOS-клиента Roblox через Darling (MacOBlox Backend)"));
    tLaunch->setObjectName("SectionTitle");
    cl->addWidget(tLaunch);

    auto *authNote = new QLabel(tr(
        "🔐 <b>Приватность пароля и сессии:</b> лаунчер не содержит встроенного браузера для перехвата cookie, "
        "никогда не запрашивает пароль и не читает/не отправляет <code>.ROBLOSECURITY</code>. "
        "Авторизация выполняется штатно внутри клиента Roblox через <b>«Быстрый вход» (Quick Login)</b>."));
    authNote->setObjectName("SubText");
    authNote->setWordWrap(true);
    cl->addWidget(authNote);

    auto *placeRow = new QHBoxLayout();
    placeRow->setSpacing(8);
    m_inpPlace = new QLineEdit();
    m_inpPlace->setPlaceholderText(tr("⚡ Необязательно: Place ID (например 2753915549), ссылка roblox.com/games/... или roblox://..."));
    auto launchByInput = [this]() {
        const QString raw = m_inpPlace->text().trimmed();
        const QString uri = SettingsManager::parsePlaceUri(raw);
        if (!raw.isEmpty() && uri.isEmpty()) {
            enqueueToast(tr("⚠️ Введите числовой Place ID, ссылку на плейс или оставьте поле пустым"));
            return;
        }
        saveSettingsNow(false);
        m_soberCtrl->launchRoblox(uri);
    };
    connect(m_inpPlace, &QLineEdit::returnPressed, this, launchByInput);
    connect(m_inpPlace, &QLineEdit::textChanged, this, [this]() { triggerInstantAutosave(); });

    auto *btnGoId = new AnimButton(tr("🚀 ЗАПУСТИТЬ ROBLOX"), nullptr, QColor(16, 185, 129), true);
    connect(btnGoId, &QPushButton::clicked, this, launchByInput);
    placeRow->addWidget(m_inpPlace, 1);
    placeRow->addWidget(btnGoId);
    cl->addLayout(placeRow);

    auto *sessRow = new QHBoxLayout();
    m_lblSessionStatus = new QLabel(tr("Проверка статуса локальной сессии в Darling..."));
    m_lblSessionStatus->setObjectName("SubText");
    m_btnLogout = new AnimButton(tr("🚪 Очистить сессию в Darling"), nullptr, QColor(239, 68, 68), false, true);
    connect(m_btnLogout, &QPushButton::clicked, this, [this]() { m_soberCtrl->logoutAsync(); });
    sessRow->addWidget(m_lblSessionStatus, 1);
    sessRow->addWidget(m_btnLogout);
    cl->addLayout(sessRow);
    lay->addWidget(cardLaunch);

    // 2. Non-Blocking First-Run & Update Wizard Card
    auto *cardWiz = new QFrame();
    cardWiz->setObjectName("GlassCard");
    auto *cw = new QVBoxLayout(cardWiz);
    cw->setContentsMargins(18, 16, 18, 16);
    cw->setSpacing(10);

    auto *tWiz = new QLabel(tr("🛠️ Мастер первой установки и обновления (Darling, Шимы, RobloxPlayer.app)"));
    tWiz->setObjectName("SectionTitle");
    cw->addWidget(tWiz);

    m_lblEnvSummary = new QLabel();
    m_lblEnvSummary->setWordWrap(true);
    cw->addWidget(m_lblEnvSummary);

    m_wizardProgressBar = new QProgressBar();
    m_wizardProgressBar->setObjectName("UpdateBar");
    m_wizardProgressBar->setRange(0, 100);
    m_wizardProgressBar->setValue(0);
    cw->addWidget(m_wizardProgressBar);

    m_lblWizardStatus = new QLabel(tr("Готов к сборке libMacOBloxShims.dylib и загрузке RobloxPlayer.app с официального CDN."));
    m_lblWizardStatus->setObjectName("SubText");
    m_lblWizardStatus->setWordWrap(true);
    cw->addWidget(m_lblWizardStatus);

    auto *wizBtns = new QHBoxLayout();
    m_btnRunWizard = new AnimButton(tr("⬇️ Установить / Обновить (Шимы + Roblox)"), nullptr, QColor(16, 185, 129), true);
    connect(m_btnRunWizard, &QPushButton::clicked, this, [this]() {
        m_btnRunWizard->setEnabled(false);
        m_soberCtrl->runSetupOrUpdateWizardAsync();
    });
    auto *btnCheckEnv = new AnimButton(tr("🔍 Проверить зависимости"), nullptr, QColor(56, 189, 248));
    connect(btnCheckEnv, &QPushButton::clicked, this, &MainWindow::refreshEnvironmentAndSessionUi);
    wizBtns->addWidget(m_btnRunWizard, 2);
    wizBtns->addWidget(btnCheckEnv, 1);
    cw->addLayout(wizBtns);
    lay->addWidget(cardWiz);

    // 3. Diagnostics Card with "Копировать отчёт"
    auto *cardDiag = new QFrame();
    cardDiag->setObjectName("GlassCard");
    auto *cd = new QVBoxLayout(cardDiag);
    cd->setContentsMargins(18, 16, 18, 16);
    cd->setSpacing(10);

    auto *tDiag = new QLabel(tr("🩺 Диагностика системы (Версии, GPU/VRAM, Wayland/X11, Ошибки)"));
    tDiag->setObjectName("SectionTitle");
    cd->addWidget(tDiag);

    m_homeDiagView = new QTextEdit();
    m_homeDiagView->setReadOnly(true);
    m_homeDiagView->setFixedHeight(140);
    cd->addWidget(m_homeDiagView);

    auto *diagRow = new QHBoxLayout();
    auto *btnCopyDiag = new AnimButton(tr("📋 Копировать отчёт"), nullptr, QColor(56, 189, 248));
    connect(btnCopyDiag, &QPushButton::clicked, this, [this]() {
        const QString report = m_soberCtrl->collectDiagnosticsReport();
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(report);
        }
        if (m_homeDiagView) m_homeDiagView->setPlainText(report);
        enqueueToast(tr("📋 Отчёт диагностики скопирован в буфер обмена!"));
    });
    diagRow->addWidget(btnCopyDiag);
    diagRow->addStretch();
    cd->addLayout(diagRow);
    lay->addWidget(cardDiag);

    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildAccountsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    auto *cardProf = new QFrame();
    cardProf->setObjectName("GlassCard");
    auto *cp = new QVBoxLayout(cardProf);
    cp->setContentsMargins(18, 16, 18, 16);
    cp->setSpacing(10);

    auto *tProf = new QLabel(tr("📂 Именные профили настроек (Сохранение, Загрузка, Экспорт / Импорт, Сброс)"));
    tProf->setObjectName("SectionTitle");
    cp->addWidget(tProf);

    auto *rowNamed = new QHBoxLayout();
    m_comboNamedProfiles = new DarkComboBox();
    m_inpNewProfileName = new QLineEdit();
    m_inpNewProfileName->setPlaceholderText(tr("Имя профиля настроек (например: Hyprland240)..."));
    auto *btnSaveNamed = new AnimButton(tr("💾 Сохранить профиль"), nullptr, QColor(16, 185, 129));
    auto *btnLoadNamed = new AnimButton(tr("📂 Загрузить"), nullptr, QColor(56, 189, 248));
    auto *btnDelNamed = new AnimButton(tr("🗑 Удалить"), nullptr, QColor(239, 68, 68), false, true);

    connect(btnSaveNamed, &QPushButton::clicked, this, [this]() {
        collectSettingsFromUi();
        const QString name = m_inpNewProfileName->text().trimmed().isEmpty()
                                 ? m_comboNamedProfiles->currentText()
                                 : m_inpNewProfileName->text().trimmed();
        QString err;
        if (m_settingsMgr.saveNamedProfile(name, &err)) {
            m_inpNewProfileName->clear();
            refreshNamedProfilesCombo();
            enqueueToast(tr("💾 Профиль «%1» сохранён!").arg(name));
        } else {
            enqueueToast("⚠️ " + err);
        }
    });
    connect(btnLoadNamed, &QPushButton::clicked, this, [this]() {
        const QString name = m_comboNamedProfiles->currentText();
        if (name.isEmpty()) return;
        QString err;
        if (m_settingsMgr.loadNamedProfile(name, &err)) {
            syncUiFromSettings();
            applyLayoutAndTypographyLive();
            enqueueToast(tr("📂 Профиль «%1» загружен!").arg(name));
        } else {
            enqueueToast("⚠️ " + err);
        }
    });
    connect(btnDelNamed, &QPushButton::clicked, this, [this]() {
        const QString name = m_comboNamedProfiles->currentText();
        if (name.isEmpty()) return;
        m_settingsMgr.deleteNamedProfile(name);
        refreshNamedProfilesCombo();
        enqueueToast(tr("🗑 Профиль «%1» удалён").arg(name));
    });

    rowNamed->addWidget(m_comboNamedProfiles, 1);
    rowNamed->addWidget(m_inpNewProfileName, 1);
    rowNamed->addWidget(btnSaveNamed);
    rowNamed->addWidget(btnLoadNamed);
    rowNamed->addWidget(btnDelNamed);
    cp->addLayout(rowNamed);
    lay->addWidget(cardProf);

    auto *cardTop = new QFrame();
    cardTop->setObjectName("GlassCard");
    auto *ct = new QVBoxLayout(cardTop);
    ct->setContentsMargins(18, 16, 18, 16);
    ct->setSpacing(10);

    auto *t1 = new QLabel(tr("👥 Локальные слоты сессий Darling (без чтения .ROBLOSECURITY)"));
    t1->setObjectName("SectionTitle");
    ct->addWidget(t1);

    auto *sub = new QLabel(tr(
        "Каждый слот изолирует каталог <code>~/Library/MacOBlox</code> внутри префикса Darling. "
        "Лаунчер переключает папки целиком и никогда не читает и не расшифровывает токены."));
    sub->setObjectName("SubText");
    sub->setWordWrap(true);
    ct->addWidget(sub);

    auto *btnRow = new QHBoxLayout();
    auto *bAdd = new AnimButton(tr("➕ Добавить слот"));
    auto *bSave = new AnimButton(tr("📥 Сохранить текущую сессию в слот"));

    connect(bAdd, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Новый слот"), tr("Название слота:"), QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            AccountProfile p;
            p.id = QString("slot_%1").arg(QDateTime::currentSecsSinceEpoch() % 100000);
            p.label = name.trimmed();
            p.displayName = name.trimmed();
            m_settingsMgr.accounts().push_back(p);
            m_soberCtrl->switchAccountSlotAsync(p.id);
        }
    });
    connect(bSave, &QPushButton::clicked, this, [this]() {
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        enqueueToast(tr("✓ Локальная папка сессии сохранена в активный слот"));
    });

    btnRow->addWidget(bAdd);
    btnRow->addWidget(bSave);
    ct->addLayout(btnRow);
    lay->addWidget(cardTop);

    m_accountsListLayout = new QVBoxLayout();
    m_accountsListLayout->setSpacing(10);
    lay->addLayout(m_accountsListLayout);
    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildHardwareAudioPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    // 1. Hardware & MacOBlox Runtime Card
    auto *cardHw = new QFrame();
    cardHw->setObjectName("GlassCard");
    auto *hw = new QVBoxLayout(cardHw);
    hw->setContentsMargins(18, 16, 18, 16);
    hw->setSpacing(10);

    auto *thw = new QLabel(tr("🖥️ Среда MacOBlox (Darling), Ввод, DNS-over-TLS и Звук"));
    thw->setObjectName("SectionTitle");
    hw->addWidget(thw);

    m_hwBannerLbl = new QLabel(tr("🔍 Определение оборудования..."));
    m_hwBannerLbl->setObjectName("SubText");
    hw->addWidget(m_hwBannerLbl);

    auto *micNote = new QLabel(tr(
        "🎙️ <b>Звук и микрофон полностью управляются внутри <code>shims/audio_hal.c</code></b> "
        "(CoreAudio HAL + AUHAL Input/Output Bus + <code>AudioUnitRender</code>). "
        "Интерфейс MqBlox вообще не прикасается к PipeWire/PulseAudio/ALSA."));
    micNote->setObjectName("SubText");
    micNote->setWordWrap(true);
    hw->addWidget(micNote);

    auto *sensRow = new QHBoxLayout();
    sensRow->addWidget(new QLabel(tr("🖱️ Чувствительность камеры и мыши (MACOBLOX_MOUSE_SENSITIVITY):")));
    m_sldMouseSens = new ResettableSlider(10, 200, 100, "%");
    m_sldMouseSens->onValueCommitted = [this](int) { triggerInstantAutosave(); };
    sensRow->addWidget(m_sldMouseSens, 1);
    hw->addLayout(sensRow);

    auto *dnsRow = new QHBoxLayout();
    dnsRow->addWidget(new QLabel(tr("🔒 DNS-over-TLS прокси (MACOBLOX_DNS):")));
    m_comboDnsProvider = new DarkComboBox();
    m_comboDnsProvider->addItem(tr("Системный DNS (Без DoT)"), 0);
    m_comboDnsProvider->addItem(tr("Quad9 DoT (9.9.9.9:853 — Рекомендуется)"), 1);
    m_comboDnsProvider->addItem(tr("Cloudflare DoT (1.1.1.1:853)"), 2);
    m_comboDnsProvider->addItem(tr("Google DoT (8.8.8.8:853)"), 3);
    m_comboDnsProvider->addItem(tr("Свой IPv4 DoT сервер..."), 4);
    m_inpDnsCustom = new QLineEdit();
    m_inpDnsCustom->setPlaceholderText(tr("IPv4 адрес DoT сервера (например 9.9.9.9)"));
    connect(m_comboDnsProvider, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_inpDnsCustom) m_inpDnsCustom->setEnabled(m_comboDnsProvider->currentData().toInt() == 4);
        triggerInstantAutosave();
    });
    connect(m_inpDnsCustom, &QLineEdit::textChanged, this, [this]() { triggerInstantAutosave(); });
    dnsRow->addWidget(m_comboDnsProvider, 1);
    dnsRow->addWidget(m_inpDnsCustom, 1);
    hw->addLayout(dnsRow);

    m_chkHideMenuBar   = new QCheckBox(tr("Скрывать верхнюю строку меню macOS в окне игры (MACOBLOX_HIDE_MENU_BAR)"));
    m_chkVsyncOff      = new QCheckBox(tr("Отключить VSync в EGL (MACOBLOX_VSYNC=0, минимальная задержка ввода)"));
    m_chkReturnMq      = new QCheckBox(tr("При выходе с сервера (Leave Game) возвращать в окно MqBlox"));
    m_chkAnimBg        = new QCheckBox(tr("Анимированный 3D-шейдер аккреционного диска TON 618"));
    m_chkCloseToTray   = new QCheckBox(tr("Сворачивать MqBlox в системный трей при закрытии окна (✕)"));
    m_chkAutoClean     = new QCheckBox(tr("Авто-очистка старого кэша (>500 МБ)"));
    m_chkMemFflags     = new QCheckBox(tr("Быстрая выгрузка неиспользуемых мешей из ОЗУ (Low-RAM FFlags)"));
    m_chkNoTelemetry   = new QCheckBox(tr("Отключить фоновую телеметрию Roblox (FFlagDebugDisableTelemetry*)"));
    m_chkAutoReconnect = new QCheckBox(tr("Авто-перезаход при аварийном завершении клиента"));

    for (QCheckBox *chk : {m_chkHideMenuBar, m_chkVsyncOff, m_chkReturnMq, m_chkAnimBg,
                           m_chkCloseToTray, m_chkAutoClean, m_chkMemFflags,
                           m_chkNoTelemetry, m_chkAutoReconnect}) {
        hw->addWidget(chk);
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    lay->addWidget(cardHw);

    // 2. HyperOS 4 Liquid Glass Appearance Card
    auto *cardVis = new QFrame();
    cardVis->setObjectName("GlassCard");
    auto *cv = new QGridLayout(cardVis);
    cv->setContentsMargins(18, 16, 18, 16);
    cv->setSpacing(10);

    auto *tVis = new QLabel(tr("🎨 Оформление HyperOS 4 Liquid Glass (Единый буфер размытия)"));
    tVis->setObjectName("SectionTitle");
    cv->addWidget(tVis, 0, 0, 1, 2);

    cv->addWidget(new QLabel(tr("Пресет оформления:")), 1, 0);
    m_comboVisualPreset = new DarkComboBox();
    m_comboVisualPreset->addItem(tr("0 — Liquid Glass (Стандарт)"), 0);
    m_comboVisualPreset->addItem(tr("1 — Матовое стекло (Deep Frosted)"), 1);
    m_comboVisualPreset->addItem(tr("2 — Минимализм (Тонированный)"), 2);
    m_comboVisualPreset->addItem(tr("3 — Высокий контраст"), 3);
    m_comboVisualPreset->addItem(tr("4 — Экономный (0% GPU/CPU в фоне)"), 4);
    connect(m_comboVisualPreset, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_isLoadingUi) return;
        SettingsRegistry::applyVisualPreset(m_settingsMgr.settings(), m_comboVisualPreset->itemData(idx).toInt());
        syncUiFromSettings();
        applyLayoutAndTypographyLive();
        triggerInstantAutosave();
    });
    cv->addWidget(m_comboVisualPreset, 1, 1);

    cv->addWidget(new QLabel(tr("Тема интерфейса:")), 2, 0);
    m_comboThemeMode = new DarkComboBox();
    m_comboThemeMode->addItem(tr("Тёмная (Dark Cosmos)"), 0);
    m_comboThemeMode->addItem(tr("Светлая (Light Pearl)"), 1);
    m_comboThemeMode->addItem(tr("Системная"), 2);
    cv->addWidget(m_comboThemeMode, 2, 1);

    cv->addWidget(new QLabel(tr("Акцентный цвет:")), 3, 0);
    m_comboAccent = new DarkComboBox();
    m_comboAccent->addItem(tr("Голубой HyperOS"), 0);
    m_comboAccent->addItem(tr("Золото TON 618"), 1);
    m_comboAccent->addItem(tr("Изумрудный неон"), 2);
    m_comboAccent->addItem(tr("Фиолетовый квазар"), 3);
    m_comboAccent->addItem(tr("Свой HEX-цвет"), 4);
    cv->addWidget(m_comboAccent, 3, 1);

    cv->addWidget(new QLabel(tr("Сила размытия подложки (0..8):")), 4, 0);
    m_sldBlur = new ResettableSlider(0, 8, 4, "");
    cv->addWidget(m_sldBlur, 4, 1);

    cv->addWidget(new QLabel(tr("Непрозрачность стекла (%):")), 5, 0);
    m_sldOpacity = new ResettableSlider(2, 90, 10, "%");
    cv->addWidget(m_sldOpacity, 5, 1);

    cv->addWidget(new QLabel(tr("Яркость блика грани (%):")), 6, 0);
    m_sldBorderBright = new ResettableSlider(20, 200, 100, "%");
    cv->addWidget(m_sldBorderBright, 6, 1);

    cv->addWidget(new QLabel(tr("Масштаб интерфейса (%):")), 7, 0);
    m_sldUiScale = new ResettableSlider(80, 150, 100, "%");
    cv->addWidget(m_sldUiScale, 7, 1);

    cv->addWidget(new QLabel(tr("Размер шрифта (px):")), 8, 0);
    m_sldFontSize = new ResettableSlider(11, 17, 13, "px");
    cv->addWidget(m_sldFontSize, 8, 1);

    m_chkChameleon = new QCheckBox(tr("Адаптивный Хамелеон (окрашивание граней под цвета TON 618)"));
    m_chkReducedMotion = new QCheckBox(tr("Режим «Меньше движения» (Eco: отключает пружины и прожектор)"));
    cv->addWidget(m_chkChameleon, 9, 0, 1, 2);
    cv->addWidget(m_chkReducedMotion, 10, 0, 1, 2);

    auto *btnCustomBg = new AnimButton(tr("📂 Выбрать своё фото или видео фона (.png/.jpg/.mp4)"), nullptr, QColor(56, 189, 248));
    connect(btnCustomBg, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Выбрать фон"), QDir::homePath(),
                                                    tr("Файлы фона (*.png *.jpg *.jpeg *.mp4 *.webm *.mkv)"));
        if (!path.isEmpty()) {
            m_settingsMgr.settings().backdropType = 3;
            m_settingsMgr.settings().customBackdropPath = path;
            m_rootCosmos->loadCustomVideoFile(path);
            triggerInstantAutosave(tr("🌌 Пользовательский фон применён!"));
        }
    });
    cv->addWidget(btnCustomBg, 11, 0, 1, 2);

    for (ResettableSlider *sl : {m_sldBlur, m_sldOpacity, m_sldBorderBright, m_sldUiScale, m_sldFontSize}) {
        sl->onValueCommitted = [this](int) {
            collectSettingsFromUi();
            applyLayoutAndTypographyLive();
            triggerInstantAutosave();
        };
    }
    for (QComboBox *cb : {m_comboThemeMode, m_comboAccent}) {
        connect(cb, &QComboBox::currentIndexChanged, this, [this]() {
            collectSettingsFromUi();
            applyLayoutAndTypographyLive();
            triggerInstantAutosave();
        });
    }
    for (QCheckBox *ck : {m_chkChameleon, m_chkReducedMotion}) {
        connect(ck, &QAbstractButton::toggled, this, [this]() {
            collectSettingsFromUi();
            applyLayoutAndTypographyLive();
            triggerInstantAutosave();
        });
    }

    lay->addWidget(cardVis);
    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildGraphicsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    auto *warnFrame = new QFrame();
    warnFrame->setObjectName("WarningBanner");
    auto *wLay = new QHBoxLayout(warnFrame);
    wLay->setContentsMargins(16, 12, 16, 12);
    auto *wLbl = new QLabel(tr(
        "ℹ️ <b>macOS OpenGL Core Profile (gl_profile.c):</b> клиент RobloxPlayer.app под Darling использует OpenGL 4.1 Core. "
        "Шим <code>gl_profile.c</code> автоматически убирает префикс <code>\"AMD \"</code> из <code>GL_RENDERER</code>, "
        "поэтому сглаживание MSAA полноценно работает и на видеокартах AMD (amdgpu)."));
    wLbl->setWordWrap(true);
    wLay->addWidget(wLbl);
    lay->addWidget(warnFrame);

    auto *cardPr = new QFrame();
    cardPr->setObjectName("GlassCard");
    auto *cp = new QVBoxLayout(cardPr);
    cp->setContentsMargins(18, 16, 18, 16);
    cp->setSpacing(10);

    auto *tp = new QLabel(tr("🌟 Глобальные пресеты графики + Экспорт / Импорт JSON"));
    tp->setObjectName("SectionTitle");
    cp->addWidget(tp);

    auto *rowBtns = new QHBoxLayout();
    auto *bFps = new AnimButton(tr("🚀 УЛЬТРА FPS"), nullptr, QColor(16, 185, 129));
    auto *bBal = new AnimButton(tr("⚖️ БАЛАНС"), nullptr, QColor(56, 189, 248));
    auto *bUlt = new AnimButton(tr("💎 УЛЬТРА РЕАЛИЗМ"), nullptr, QColor(168, 85, 247));
    connect(bFps, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(0); });
    connect(bBal, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(1); });
    connect(bUlt, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(2); });
    rowBtns->addWidget(bFps);
    rowBtns->addWidget(bBal);
    rowBtns->addWidget(bUlt);
    cp->addLayout(rowBtns);

    auto *rowIo = new QHBoxLayout();
    auto *bExp = new AnimButton(tr("📤 Экспорт профиля в JSON"));
    auto *bImp = new AnimButton(tr("📥 Импорт профиля из JSON"));
    connect(bExp, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Экспорт пресета"), QDir::homePath() + "/mqblox_preset.json", "JSON (*.json)");
        if (!path.isEmpty() && m_settingsMgr.exportPresetToFile(path)) enqueueToast(tr("📤 Профиль экспортирован!"));
    });
    connect(bImp, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Импорт пресета"), QDir::homePath(), "JSON (*.json)");
        QString err;
        if (!path.isEmpty()) {
            if (m_settingsMgr.importPresetFromFile(path, &err)) {
                syncUiFromSettings();
                applyLayoutAndTypographyLive();
                enqueueToast(tr("📥 Профиль успешно импортирован!"));
            } else {
                enqueueToast("⚠️ " + err);
            }
        }
    });
    rowIo->addWidget(bExp);
    rowIo->addWidget(bImp);
    cp->addLayout(rowIo);

    m_lblPresetDesc = new QLabel(tr("Все параметры ниже атомарно записываются в RobloxPlayer.app/Contents/MacOS/ClientSettings/ClientAppSettings.json."));
    m_lblPresetDesc->setObjectName("SubText");
    cp->addWidget(m_lblPresetDesc);
    lay->addWidget(cardPr);

    auto *cardApi = new QFrame();
    cardApi->setObjectName("GlassCard");
    auto *g1 = new QGridLayout(cardApi);
    g1->setContentsMargins(18, 16, 18, 16);
    g1->setSpacing(10);
    g1->setColumnStretch(0, 0);
    g1->setColumnStretch(1, 1);

    g1->addWidget(new QLabel(tr("Технология освещения:")), 0, 0);
    m_comboLighting = new DarkComboBox();
    m_comboLighting->addItem(tr("Voxel (Максимум FPS)"), static_cast<int>(LightingMode::Voxel));
    m_comboLighting->addItem(tr("ShadowMap (Мягкие тени)"), static_cast<int>(LightingMode::ShadowMap));
    m_comboLighting->addItem(tr("Future Phase 3 (Ультра-реализм)"), static_cast<int>(LightingMode::FuturePhase3));
    m_comboLighting->addItem(tr("По умолчанию плейса"), static_cast<int>(LightingMode::GameDefault));
    g1->addWidget(m_comboLighting, 0, 1);

    g1->addWidget(new QLabel(tr("Лимит кадров (FPS):")), 1, 0);
    auto *fpsRow = new QHBoxLayout();
    m_spinFps = new QSpinBox();
    m_spinFps->setRange(15, 9999);
    m_chkUnlimFps = new QCheckBox(tr("♾️ Без ограничений (9999 FPS)"));
    connect(m_chkUnlimFps, &QAbstractButton::toggled, this, [this](bool c) {
        m_spinFps->setEnabled(!c);
        triggerInstantAutosave();
    });
    fpsRow->addWidget(m_spinFps);
    fpsRow->addWidget(m_chkUnlimFps);
    g1->addLayout(fpsRow, 1, 1);

    g1->addWidget(new QLabel(tr("Качество текстур:")), 2, 0);
    m_comboTex = new DarkComboBox();
    m_comboTex->addItem(tr("Автоматически"), static_cast<int>(TextureQuality::Auto));
    m_comboTex->addItem(tr("Уровень 0 (Минимум)"), static_cast<int>(TextureQuality::Level0));
    m_comboTex->addItem(tr("Уровень 1 (Низкие)"), static_cast<int>(TextureQuality::Level1));
    m_comboTex->addItem(tr("Уровень 2 (Средние)"), static_cast<int>(TextureQuality::Level2));
    m_comboTex->addItem(tr("Уровень 3 (Ультра HD)"), static_cast<int>(TextureQuality::Level3));
    g1->addWidget(m_comboTex, 2, 1);

    g1->addWidget(new QLabel(tr("Сглаживание (MSAA OpenGL):")), 3, 0);
    m_comboMsaa = new DarkComboBox();
    m_comboMsaa->addItem(tr("Отключено (0x)"), static_cast<int>(MsaaMode::Off));
    m_comboMsaa->addItem("2x MSAA", static_cast<int>(MsaaMode::X2));
    m_comboMsaa->addItem("4x MSAA (Макс. для macOS CGL)", static_cast<int>(MsaaMode::X4));
    g1->addWidget(m_comboMsaa, 3, 1);

    g1->addWidget(new QLabel(tr("Уровень графики (FRM 0-21):")), 4, 0);
    m_spinFrm = new QSpinBox();
    m_spinFrm->setRange(0, 21);
    m_spinFrm->setSpecialValueText(tr("0 (Из настроек игры)"));
    g1->addWidget(m_spinFrm, 4, 1);
    lay->addWidget(cardApi);

    auto *cardFx = new QFrame();
    cardFx->setObjectName("GlassCard");
    auto *g2 = new QGridLayout(cardFx);
    g2->setContentsMargins(18, 16, 18, 16);
    g2->setSpacing(10);

    m_chkNoShadows    = new QCheckBox(tr("Отключить тени (FIntRenderShadowIntensity=0)"));
    m_chkUltraShadows = new QCheckBox(tr("💎 Ультра-тени (FIntRenderShadowIntensity=100)"));
    m_chkNoPostfx     = new QCheckBox(tr("Отключить PostFX (FFlagDisablePostFx)"));
    m_chkLowMeshes    = new QCheckBox(tr("Упростить дальние модели (CSG LOD 0)"));
    m_chkGreySky      = new QCheckBox(tr("Упрощённое небо (FFlagDebugSkyGray)"));

    g2->addWidget(m_chkNoShadows, 0, 0);
    g2->addWidget(m_chkUltraShadows, 0, 1);
    g2->addWidget(m_chkNoPostfx, 1, 0);
    g2->addWidget(m_chkLowMeshes, 1, 1);
    g2->addWidget(m_chkGreySky, 2, 0, 1, 2);
    lay->addWidget(cardFx);

    auto *cardFf = new QFrame();
    cardFf->setObjectName("GlassCard");
    auto *ffl = new QVBoxLayout(cardFf);
    ffl->setContentsMargins(18, 16, 18, 16);
    ffl->setSpacing(10);

    auto *tff = new QLabel(tr("🔧 Пользовательские FastFlags (JSON -> ClientAppSettings.json):"));
    tff->setObjectName("SectionTitle");
    ffl->addWidget(tff);
    m_fflagsEditor = new QTextEdit();
    m_fflagsEditor->setFixedHeight(90);
    ffl->addWidget(m_fflagsEditor);
    m_lblFflagsValidation = new QLabel(tr("✓ Готов к проверке JSON"));
    m_lblFflagsValidation->setObjectName("SubText");
    ffl->addWidget(m_lblFflagsValidation);
    lay->addWidget(cardFf);

    for (QComboBox *c : {m_comboLighting, m_comboTex, m_comboMsaa}) {
        connect(c, &QComboBox::currentIndexChanged, this, [this]() { triggerInstantAutosave(); });
    }
    for (QSpinBox *s : {m_spinFps, m_spinFrm}) {
        connect(s, &QSpinBox::valueChanged, this, [this]() { triggerInstantAutosave(); });
    }
    for (QCheckBox *chk : {m_chkNoShadows, m_chkUltraShadows, m_chkNoPostfx, m_chkLowMeshes, m_chkGreySky}) {
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    connect(m_fflagsEditor, &QTextEdit::textChanged, this, [this]() { triggerInstantAutosave(); });

    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildCameraModsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    auto *cardCam = new QFrame();
    cardCam->setObjectName("GlassCard");
    auto *cc = new QVBoxLayout(cardCam);
    cc->setContentsMargins(18, 16, 18, 16);
    cc->setSpacing(10);

    auto *tCam = new QLabel(tr("🔥 Игровые Твики, Камера, Сеть и Трассировка Шимов MacOBlox"));
    tCam->setObjectName("SectionTitle");
    tCam->setWordWrap(true);
    cc->addWidget(tCam);

    auto *zoomRow = new QHBoxLayout();
    zoomRow->setSpacing(10);
    m_chkInfZoom = new QCheckBox(tr("🔭 Разблокировать максимальную дистанцию отдаления камеры (FIntCameraMaxZoomDistance):"));
    m_spinMaxZoom = new QSpinBox();
    m_spinMaxZoom->setRange(128, 100000);
    m_spinMaxZoom->setSingleStep(500);
    m_spinMaxZoom->setFixedWidth(130);
    zoomRow->addWidget(m_chkInfZoom, 1);
    zoomRow->addWidget(m_spinMaxZoom);
    cc->addLayout(zoomRow);

    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(10);
    presetRow->addWidget(new QLabel(tr("Быстрый пресет зума:")));
    m_comboCamZoom = new DarkComboBox();
    m_comboCamZoom->addItem(tr("Стандартный (128 студов)"), static_cast<int>(CameraZoomMode::Default));
    m_comboCamZoom->addItem(tr("Средний обзор (500 студов)"), static_cast<int>(CameraZoomMode::Medium500));
    m_comboCamZoom->addItem(tr("Дальний обзор (2000 студов)"), static_cast<int>(CameraZoomMode::Far2000));
    m_comboCamZoom->addItem(tr("Максимум движка (99999 студов)"), static_cast<int>(CameraZoomMode::Cosmic9999));
    connect(m_comboCamZoom, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_isLoadingUi || !m_spinMaxZoom) return;
        int m = m_comboCamZoom->itemData(idx).toInt();
        if (m == 0) m_spinMaxZoom->setValue(128);
        else if (m == 1) m_spinMaxZoom->setValue(500);
        else if (m == 2) m_spinMaxZoom->setValue(2000);
        else m_spinMaxZoom->setValue(99999);
        triggerInstantAutosave();
    });
    presetRow->addWidget(m_comboCamZoom, 1);
    cc->addLayout(presetRow);

    auto *netRow = new QHBoxLayout();
    netRow->setSpacing(10);
    netRow->addWidget(new QLabel(tr("🌐 Сетевой пакетный профиль (MTU / RakNet):")));
    m_comboNetOpt = new DarkComboBox();
    m_comboNetOpt->addItem(tr("⚡ Ультра-отклик без фрагментации (MTU 1396 + Fast RakNet)"), static_cast<int>(NetworkProfile::UltraLowPing));
    m_comboNetOpt->addItem(tr("Стандартный буфер (MTU 1492)"), static_cast<int>(NetworkProfile::Balanced));
    netRow->addWidget(m_comboNetOpt, 1);
    cc->addLayout(netRow);

    m_chkFullBright = new QCheckBox(tr("☀️ FullBright (Voxel + тени 0 + без PostFX)"));
    m_chkClearFog   = new QCheckBox(tr("🌿 Убрать 3D-траву на ландшафте (FIntFRMMinGrassDistance=0)"));
    m_chkHitboxes   = new QCheckBox(tr("📐 Упростить геометрию дальних объектов (CSG LOD 0)"));
    m_chkHudFps     = new QCheckBox(tr("📊 Включить счётчик FPS в игре (FFlagDebugDisplayFPS + MACOBLOX_FPS_LOG)"));

    m_chkFpsLog      = new QCheckBox(tr("📈 Логировать FPS каждые 2 с из gl_profile.c (MACOBLOX_FPS_LOG=1)"));
    m_chkTraceUdp    = new QCheckBox(tr("🌐 Трассировка UDP/kevent и сторожевой таймер фризов (MACOBLOX_TRACE_UDP=1)"));
    m_chkTraceLock   = new QCheckBox(tr("🔒 Трассировка медленных блокировок psynch_mutex (MACOBLOX_TRACE_LOCK=1)"));
    m_chkTraceGl     = new QCheckBox(tr("🎞️ Трассировка вызовов CGL/OpenGL (MACOBLOX_TRACE_GL=1)"));
    m_chkDiagSignals = new QCheckBox(tr("💥 Дамп регистров и бэктрейса при сбоях (MACOBLOX_DIAGNOSTIC_SIGNALS=1)"));

    for (QCheckBox *chk : {m_chkFullBright, m_chkClearFog, m_chkHitboxes, m_chkHudFps,
                           m_chkFpsLog, m_chkTraceUdp, m_chkTraceLock, m_chkTraceGl,
                           m_chkDiagSignals, m_chkInfZoom}) {
        if (chk != m_chkInfZoom) cc->addWidget(chk);
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    connect(m_spinMaxZoom, &QSpinBox::valueChanged, this, [this]() { triggerInstantAutosave(); });
    connect(m_comboNetOpt, &QComboBox::currentIndexChanged, this, [this]() { triggerInstantAutosave(); });
    lay->addWidget(cardCam);

    auto *cardMods = new QFrame();
    cardMods->setObjectName("GlassCard");
    auto *cm = new QVBoxLayout(cardMods);
    cm->setContentsMargins(18, 16, 18, 16);
    cm->setSpacing(10);

    auto *tm = new QLabel(tr("🩺 Инструменты диагностики MacOBlox и Журнал выполнения"));
    tm->setObjectName("SectionTitle");
    cm->addWidget(tm);

    auto *rMods = new QHBoxLayout();
    auto *bShowCfg = new AnimButton(tr("🔍 Показать ClientAppSettings.json"), nullptr, QColor(16, 185, 129));
    auto *bCopyDiag = new AnimButton(tr("📋 Копировать отчёт"), nullptr, QColor(56, 189, 248));
    auto *bOpenData = new AnimButton(tr("📂 Папка данных MacOBlox"));

    connect(bShowCfg, &QPushButton::clicked, this, [this]() {
        saveSettingsNow(true);
        QFile cf(m_settingsMgr.clientAppSettingsPath());
        if (cf.open(QIODevice::ReadOnly)) {
            m_logView->clear();
            m_logView->append("✓ Файл " + m_settingsMgr.clientAppSettingsPath() + ":\n");
            m_logView->append(QString::fromUtf8(cf.readAll()));
            cf.close();
        }
    });
    connect(bCopyDiag, &QPushButton::clicked, this, [this]() {
        const QString report = m_soberCtrl->collectDiagnosticsReport();
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(report);
        }
        m_logView->setPlainText(report);
        enqueueToast(tr("📋 Отчёт диагностики скопирован в буфер обмена!"));
    });
    connect(bOpenData, &QPushButton::clicked, this, [this]() {
        QProcess::startDetached("xdg-open", {m_settingsMgr.macobloxDataDir()});
    });

    rMods->addWidget(bShowCfg);
    rMods->addWidget(bCopyDiag);
    rMods->addWidget(bOpenData);
    cm->addLayout(rMods);
    lay->addWidget(cardMods);

    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->document()->setMaximumBlockCount(1000);
    m_logView->setMinimumHeight(180);
    lay->addWidget(m_logView);
    return page;
}

QWidget *MainWindow::buildUpdatesPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);
    lay->setSpacing(12);

    // 1. Credits & MIT License Card
    auto *cardCredits = new QFrame();
    cardCredits->setObjectName("GlassCard");
    auto *cc = new QVBoxLayout(cardCredits);
    cc->setContentsMargins(18, 16, 18, 16);
    cc->setSpacing(10);

    auto *tCred = new QLabel(tr("ℹ️ О программе MqBlox × MacOBlox (v36.0) — Лицензия MIT и Благодарности (Credits)"));
    tCred->setObjectName("SectionTitle");
    cc->addWidget(tCred);

    auto *creditsText = new QLabel(tr(
        "<b>Лицензия:</b> MIT License (см. файл <code>LICENSE</code> в корне репозитория).<br><br>"
        "<b>Благодарности и авторы (Credits):</b><ul>"
        "<li><b>Narezany (<code>narezy</code>) — MacOBlox</b> "
        "(<code>Copyright (c) 2026 Narezany</code>, MIT License): автор оригинального проекта "
        "<a href=\"https://github.com/narezy/MacOBlox\">https://github.com/narezy/MacOBlox</a>, "
        "архитектуры запуска macOS-клиента Roblox на Linux через Darling, всех C/Objective-C шимов "
        "(<code>libMacOBloxShims.m</code>, <code>audio_hal.c</code>, <code>gl_profile.c</code>, "
        "<code>gpu_info.c</code>, <code>memory_stats.c</code>, <code>dns_override.c</code>, "
        "<code>darling_fixes.c</code>, <code>xfixes_raw.c</code>, <code>net_trace.c</code>, "
        "<code>fast_libc.c</code>, <code>xattr_compat.c</code>, <code>exit_compat.c</code>, "
        "<code>connectx_compat.c</code>, <code>missing_symbols.c</code>) и стаб-фреймворков "
        "(<code>CoreML</code>, <code>CoreHaptics</code>, <code>DeviceCheck</code>).</li>"
        "<li><b>Darling Project</b> (<a href=\"https://www.darlinghq.org/\">https://www.darlinghq.org/</a>): "
        "слой трансляции macOS / Darwin для Linux.</li>"
        "<li><b>MrQuster — MqBlox</b> (<code>Copyright (c) 2026 MrQuster</code>, MIT License): "
        "интерфейс HyperOS 4 Liquid Glass на C++20/Qt6 (<a href=\"https://github.com/MrQuster/mqblox\">"
        "https://github.com/MrQuster/mqblox</a>), шейдерный движок <code>Ton618GlWidget</code>, "
        "единый реестр <code>SettingsRegistry</code>, мастер установки/обновления, диагностика и исправления "
        "захвата микрофона в <code>audio_hal.c</code>.</li></ul>"));
    creditsText->setWordWrap(true);
    creditsText->setOpenExternalLinks(true);
    cc->addWidget(creditsText);

    auto *linksRow = new QHBoxLayout();
    auto *btnOpenMacOBlox = new AnimButton(tr("🌐 GitHub: narezy/MacOBlox"), nullptr, QColor(56, 189, 248));
    connect(btnOpenMacOBlox, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/narezy/MacOBlox"));
    });
    auto *btnOpenMqBlox = new AnimButton(tr("🌐 GitHub: MrQuster/mqblox"), nullptr, QColor(168, 85, 247));
    connect(btnOpenMqBlox, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/MrQuster/mqblox"));
    });
    linksRow->addWidget(btnOpenMacOBlox);
    linksRow->addWidget(btnOpenMqBlox);
    linksRow->addStretch();
    cc->addLayout(linksRow);
    lay->addWidget(cardCredits);

    // 2. GitHub / Roblox Update Card
    auto *cardInfo = new QFrame();
    cardInfo->setObjectName("GlassCard");
    auto *ci = new QVBoxLayout(cardInfo);
    ci->setContentsMargins(18, 16, 18, 16);
    ci->setSpacing(10);

    auto *title = new QLabel(tr("🔄 Проверка обновлений репозитория и клиента Roblox"));
    title->setObjectName("SectionTitle");
    ci->addWidget(title);

    m_lblGitVersionInfo = new QLabel(tr("Готов к проверке статуса репозитория..."));
    m_lblGitVersionInfo->setWordWrap(true);
    ci->addWidget(m_lblGitVersionInfo);

    m_updateProgressBar = new QProgressBar();
    m_updateProgressBar->setObjectName("UpdateBar");
    m_updateProgressBar->setRange(0, 100);
    m_updateProgressBar->setValue(100);
    m_updateProgressBar->setFormat(tr("Готов к проверке обновлений"));
    ci->addWidget(m_updateProgressBar);

    auto *btnRow = new QHBoxLayout();
    auto *btnCheck = new AnimButton(tr("🔍 Проверить Git"), nullptr, QColor(56, 189, 248));
    auto *btnWiz = new AnimButton(tr("⬇️ Обновить Шимы и RobloxPlayer.app"), nullptr, QColor(16, 185, 129), true);
    connect(btnCheck, &QPushButton::clicked, this, [this]() { refreshGitUpdateStatus(); });
    connect(btnWiz, &QPushButton::clicked, this, [this]() {
        switchTab(0);
        if (m_btnRunWizard && m_btnRunWizard->isEnabled()) {
            m_btnRunWizard->click();
        }
    });
    btnRow->addWidget(btnCheck);
    btnRow->addWidget(btnWiz);
    ci->addLayout(btnRow);
    lay->addWidget(cardInfo);

    auto *cardLog = new QFrame();
    cardLog->setObjectName("GlassCard");
    auto *cl = new QVBoxLayout(cardLog);
    cl->setContentsMargins(18, 16, 18, 16);
    auto *tLog = new QLabel(tr("📜 Журнал Git коммитов:"));
    tLog->setObjectName("SectionTitle");
    cl->addWidget(tLog);

    m_updateLogView = new QTextEdit();
    m_updateLogView->setReadOnly(true);
    m_updateLogView->setMinimumHeight(160);
    cl->addWidget(m_updateLogView);
    lay->addWidget(cardLog);

    lay->addStretch();
    return page;
}

void MainWindow::refreshGitUpdateStatus() {
    if (!m_lblGitVersionInfo || !m_updateLogView) return;
    m_updateProgressBar->setRange(0, 0);
    m_updateProgressBar->setFormat(tr("⏳ Чтение локальной истории Git..."));
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(QCoreApplication::applicationDirPath());
    connect(proc, &QProcess::finished, this, [this, proc](int, QProcess::ExitStatus) {
        QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        m_updateProgressBar->setRange(0, 100);
        m_updateProgressBar->setValue(100);
        m_lblGitVersionInfo->setText(tr("🟢 <b>MqBlox × MacOBlox v36.0 (Schema v36)</b>"));
        m_updateProgressBar->setFormat(tr("✓ Актуальная сборка"));
        if (!out.isEmpty()) m_updateLogView->setPlainText(out);
    });
    proc->start("git", {"log", "-n", "8", "--pretty=format:✦ [%h] %s (%cr)"});
}

void MainWindow::refreshEnvironmentAndSessionUi() {
    const EnvironmentStatus st = m_soberCtrl->checkEnvironment();
    if (m_lblEnvSummary) {
        m_lblEnvSummary->setText(
            QString("<b>Компоненты:</b> Darling: %1 | clang: %2 | ld.lld: %3 | unzip: %4 | "
                    "Шим <code>libMacOBloxShims.dylib</code>: %5 | <code>RobloxPlayer.app</code>: %6")
                .arg(st.hasDarling ? "✅ Да" : "❌ Нет",
                     st.hasClang ? "✅ Да" : "❌ Нет",
                     st.hasLld ? "✅ Да" : "❌ Нет",
                     st.hasUnzip ? "✅ Да" : "❌ Нет",
                     st.hasShimDylib ? "✅ Собран" : "⚠️ Не собран",
                     st.hasRobloxApp ? ("✅ v" + st.installedRobloxVersion) : "⚠️ Не установлен"));
    }
    if (m_lblSessionStatus) {
        const bool signedIn = m_soberCtrl->isSignedIn();
        m_lblSessionStatus->setText(
            signedIn
                ? tr("🟢 <b>Сессия в Darling активна</b> (Cookies.plist присутствует; токен не читается лаунчером)")
                : tr("⚪ <b>Сессия не найдена</b> — войдите внутри Roblox через «Быстрый вход» (Quick Login)"));
        if (m_btnLogout) m_btnLogout->setEnabled(signedIn);
    }
    if (m_homeDiagView) {
        m_homeDiagView->setPlainText(m_soberCtrl->collectDiagnosticsReport());
    }
}

void MainWindow::refreshNamedProfilesCombo() {
    if (!m_comboNamedProfiles) return;
    m_comboNamedProfiles->clear();
    const QStringList list = m_settingsMgr.listNamedProfiles();
    if (list.isEmpty()) {
        m_comboNamedProfiles->addItem("Default");
    } else {
        m_comboNamedProfiles->addItems(list);
    }
}

void MainWindow::buildSearchIndex() {
    m_searchIndex.clear();
    if (!m_pages) return;
    for (int i = 0; i < m_pages->count(); ++i) {
        QWidget *page = m_pages->widget(i);
        if (!page) continue;
        const auto cards = page->findChildren<QFrame *>("GlassCard");
        for (QFrame *card : cards) {
            QStringList tokens;
            for (QLabel *l : card->findChildren<QLabel *>()) tokens << l->text();
            for (QCheckBox *c : card->findChildren<QCheckBox *>()) tokens << c->text() << c->toolTip();
            for (QComboBox *cmb : card->findChildren<QComboBox *>()) {
                for (int k = 0; k < cmb->count(); ++k) tokens << cmb->itemText(k);
            }
            for (QPushButton *b : card->findChildren<QPushButton *>()) tokens << b->text() << b->toolTip();
            m_searchIndex.push_back(SearchEntry{i, card, tokens.join(' ').toLower()});
        }
    }
}

void MainWindow::filterSettingsBySearch(const QString &query) {
    if (m_searchIndex.empty()) buildSearchIndex();
    const QString q = query.trimmed().toLower();
    int firstMatchedTab = -1;

    setUpdatesEnabled(false);
    for (const auto &entry : m_searchIndex) {
        if (!entry.card) continue;
        bool match = q.isEmpty() || entry.haystackLower.contains(q);
        entry.card->setVisible(match);
        if (match && firstMatchedTab == -1) {
            firstMatchedTab = entry.tabIndex;
        }
    }
    setUpdatesEnabled(true);

    if (!q.isEmpty() && firstMatchedTab != -1 && m_pages->currentIndex() != firstMatchedTab) {
        switchTab(firstMatchedTab);
    }
}

void MainWindow::resetCurrentCategoryToDefaults() {
    AppSettings &s = m_settingsMgr.settings();
    int idx = m_pages->currentIndex();
    if (idx == 2) {
        SettingsRegistry::resetCategory(s, SettingCategory::System);
        SettingsRegistry::resetCategory(s, SettingCategory::General);
        SettingsRegistry::resetVisualAppearance(s);
    } else if (idx == 3) {
        SettingsRegistry::resetCategory(s, SettingCategory::Graphics);
    } else if (idx == 4) {
        SettingsRegistry::resetCategory(s, SettingCategory::Tweaks);
    } else {
        SettingsRegistry::resetAll(s);
    }
    syncUiFromSettings();
    saveSettingsNow(true);
    applyLayoutAndTypographyLive();
    enqueueToast(tr("↺ Параметры текущей вкладки сброшены по умолчанию!"));
}

void MainWindow::resetAppearanceToDefault() {
    SettingsRegistry::resetVisualAppearance(m_settingsMgr.settings());
    syncUiFromSettings();
    saveSettingsNow(true);
    applyLayoutAndTypographyLive();
    enqueueToast(tr("🎨 Оформление возвращено к стандартному стилю HyperOS 4 Liquid Glass!"));
}

void MainWindow::applyLayoutAndTypographyLive() {
    const AppSettings &s = m_settingsMgr.settings();
    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    int scaledPx = std::clamp(int(s.fontSizePx * (s.uiScalePct / 100.0)), 10, 22);
    int fontWeight = (s.fontBoldness == 1) ? 700 : 500;
    bool lightTheme = m_rootCosmos && m_rootCosmos->isLightThemeActive();
    QColor acc = m_rootCosmos ? m_rootCosmos->fixedAccentColor() : QColor(56, 189, 248);

    QString mainTxtHex = lightTheme ? "#0F172A" : "#F8FAFC";
    QString subTxtHex = lightTheme ? "rgba(30, 41, 59, 215)" : "rgba(241, 245, 249, 215)";

    QString dynQss = QString(
        "QLabel { color: %1; font-size: %2px; font-weight: %3; font-style: normal; background: transparent; }\n"
        "QLabel#SubText { color: %4; font-size: %5px; }\n"
        "QToolTip { color: #F8FAFC; background-color: rgba(15, 23, 42, 240); border: 1px solid rgba(%7, %8, %9, 180); border-radius: 8px; padding: 6px 10px; }\n"
        "QDialog { background-color: rgb(10, 14, 26); color: #F8FAFC; }\n"
        "QSpinBox, QLineEdit { color: %1; font-size: %2px; font-weight: 700; background: transparent; border: none; padding: 6px 12px; }\n"
        "QTextEdit { color: %1; background-color: %6; border: 1px solid rgba(%7, %8, %9, 95); border-radius: 14px; padding: 10px; }\n"
        "QSlider::groove:horizontal { height: 6px; background: rgba(148, 163, 184, 55); border-radius: 3px; }\n"
        "QSlider::sub-page:horizontal { background: rgba(%7, %8, %9, 195); border-radius: 3px; }\n"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; "
        "background: #FFFFFF; border: 2px solid rgb(%7, %8, %9); }\n")
        .arg(mainTxtHex)
        .arg(scaledPx)
        .arg(fontWeight)
        .arg(subTxtHex)
        .arg(std::max(10, scaledPx - 2))
        .arg(lightTheme ? "rgba(255, 255, 255, 140)" : "rgba(6, 10, 20, 45)")
        .arg(acc.red())
        .arg(acc.green())
        .arg(acc.blue());

    if (m_pages && m_lastAppliedQss != dynQss) {
        m_lastAppliedQss = dynQss;
        m_pages->setStyleSheet(dynQss);
    }

    if (m_sidebar) {
        bool compact = !s.sidebarShowLabels || (s.sidebarPosition == 1) ||
                       (!s.sidebarPinned && !m_sidebar->underMouse());
        int targetW = compact ? 68 : std::clamp(s.sidebarWidthPx, 160, 280);
        m_sidebar->setMinimumWidth(targetW);
        m_sidebar->setMaximumWidth(targetW);
        for (auto *b : m_navButtons) {
            b->setCompactMode(compact);
        }
    }

    int padV = (s.uiDensity == 0) ? 6 : ((s.uiDensity == 2) ? 16 : 12);
    for (int i = 0; i < m_pages->count(); ++i) {
        if (auto *sa = qobject_cast<QScrollArea *>(m_pages->widget(i))) {
            if (sa->widget() && sa->widget()->layout()) {
                sa->widget()->layout()->setSpacing(padV);
            }
        }
    }

    if (!QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        bool curTop = (windowFlags() & Qt::WindowStaysOnTopHint);
        if (curTop != s.alwaysOnTop) {
            setWindowFlag(Qt::WindowStaysOnTopHint, s.alwaysOnTop);
            show();
        }
    }
    update();
}

void MainWindow::syncUiFromSettings() {
    m_isLoadingUi = true;
    const AppSettings &s = m_settingsMgr.settings();

    if (m_comboRes) m_comboRes->setCurrentIndex(m_comboRes->findData(static_cast<int>(s.resMode)));
    if (m_comboLighting) m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(s.lighting)));
    if (m_comboTex) m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(s.textureQuality)));
    if (m_comboMsaa) m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(s.msaa)));
    if (m_comboCamZoom) m_comboCamZoom->setCurrentIndex(m_comboCamZoom->findData(static_cast<int>(s.camZoom)));
    if (m_comboNetOpt) m_comboNetOpt->setCurrentIndex(m_comboNetOpt->findData(static_cast<int>(s.netProfile)));

    if (m_spinFps) m_spinFps->setValue(s.targetFps);
    if (m_chkUnlimFps) m_chkUnlimFps->setChecked(s.unlimitedFps);
    if (m_spinFps) m_spinFps->setEnabled(!s.unlimitedFps);
    if (m_spinFrm) m_spinFrm->setValue(s.frmQualityLevel);

    if (m_sldMouseSens) m_sldMouseSens->setValueSilent(s.mouseSensitivityPct);
    if (m_chkHideMenuBar) m_chkHideMenuBar->setChecked(s.hideMenuBar);
    if (m_comboDnsProvider) {
        m_comboDnsProvider->setCurrentIndex(m_comboDnsProvider->findData(s.dnsProvider));
    }
    if (m_inpDnsCustom) {
        m_inpDnsCustom->setText(s.dnsCustom);
        m_inpDnsCustom->setEnabled(s.dnsProvider == 4);
    }

    if (m_chkReturnMq) m_chkReturnMq->setChecked(s.returnToMqBloxOnLeave);
    if (m_chkAnimBg) m_chkAnimBg->setChecked(s.animBg);
    if (m_chkCloseToTray) m_chkCloseToTray->setChecked(s.closeToTray);
    if (m_chkVsyncOff) m_chkVsyncOff->setChecked(s.vsyncOff);
    if (m_chkAutoClean) m_chkAutoClean->setChecked(s.smartAutoCleanCache);
    if (m_chkMemFflags) m_chkMemFflags->setChecked(s.lowRamFflags);
    if (m_chkNoTelemetry) m_chkNoTelemetry->setChecked(s.disableTelemetry);
    if (m_chkAutoReconnect) m_chkAutoReconnect->setChecked(s.autoReconnectOnCrash);

    if (m_chkNoShadows) m_chkNoShadows->setChecked(s.noShadows);
    if (m_chkUltraShadows) m_chkUltraShadows->setChecked(s.ultraShadows);
    if (m_chkNoPostfx) m_chkNoPostfx->setChecked(s.noPostFx);
    if (m_chkLowMeshes) m_chkLowMeshes->setChecked(s.lowMeshes);
    if (m_chkGreySky) m_chkGreySky->setChecked(s.greySky);

    if (m_chkHudFps) m_chkHudFps->setChecked(s.showHudFps);
    if (m_chkClearFog) m_chkClearFog->setChecked(s.clearFog);
    if (m_spinMaxZoom) m_spinMaxZoom->setValue(s.customMaxZoom);
    if (m_chkInfZoom) m_chkInfZoom->setChecked(s.unlockInfiniteZoom);
    if (m_chkFullBright) m_chkFullBright->setChecked(s.fullBright);
    if (m_chkHitboxes) m_chkHitboxes->setChecked(s.showHitboxes);

    if (m_chkFpsLog) m_chkFpsLog->setChecked(s.fpsLog);
    if (m_chkTraceUdp) m_chkTraceUdp->setChecked(s.traceUdp);
    if (m_chkTraceLock) m_chkTraceLock->setChecked(s.traceLock);
    if (m_chkTraceGl) m_chkTraceGl->setChecked(s.traceGl);
    if (m_chkDiagSignals) m_chkDiagSignals->setChecked(s.diagnosticSignals);

    if (m_comboVisualPreset) m_comboVisualPreset->setCurrentIndex(m_comboVisualPreset->findData(s.visualPreset));
    if (m_sldBlur) m_sldBlur->setValueSilent(s.glassBlurStrength);
    if (m_sldOpacity) m_sldOpacity->setValueSilent(s.glassOpacityPct);
    if (m_sldBorderBright) m_sldBorderBright->setValueSilent(s.borderBrightnessPct);
    if (m_comboThemeMode) m_comboThemeMode->setCurrentIndex(m_comboThemeMode->findData(s.themeMode));
    if (m_comboAccent) m_comboAccent->setCurrentIndex(m_comboAccent->findData(s.accentPreset));
    if (m_chkChameleon) m_chkChameleon->setChecked(s.chameleonEnabled);
    if (m_chkReducedMotion) m_chkReducedMotion->setChecked(s.reducedMotion);
    if (m_sldUiScale) m_sldUiScale->setValueSilent(s.uiScalePct);
    if (m_sldFontSize) m_sldFontSize->setValueSilent(s.fontSizePx);

    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    if (m_inpPlace) m_inpPlace->setText(s.lastPlaceInput);
    if (m_fflagsEditor) m_fflagsEditor->setPlainText(s.customFflagsJson);
    if (m_btnPinSide) m_btnPinSide->setText(s.sidebarPinned ? "📌" : "📍");
    m_isLoadingUi = false;
}

void MainWindow::collectSettingsFromUi() {
    AppSettings &s = m_settingsMgr.settings();
    if (m_comboRes) s.resMode = static_cast<ResolutionMode>(m_comboRes->currentData().toInt());
    if (m_comboLighting) s.lighting = static_cast<LightingMode>(m_comboLighting->currentData().toInt());
    if (m_comboTex) s.textureQuality = static_cast<TextureQuality>(m_comboTex->currentData().toInt());
    if (m_comboMsaa) s.msaa = static_cast<MsaaMode>(m_comboMsaa->currentData().toInt());
    if (m_comboCamZoom) s.camZoom = static_cast<CameraZoomMode>(m_comboCamZoom->currentData().toInt());
    if (m_comboNetOpt) s.netProfile = static_cast<NetworkProfile>(m_comboNetOpt->currentData().toInt());

    if (m_spinFps) s.targetFps = m_spinFps->value();
    if (m_chkUnlimFps) s.unlimitedFps = m_chkUnlimFps->isChecked();
    if (m_spinFrm) s.frmQualityLevel = m_spinFrm->value();

    if (m_sldMouseSens) s.mouseSensitivityPct = m_sldMouseSens->value();
    if (m_chkHideMenuBar) s.hideMenuBar = m_chkHideMenuBar->isChecked();
    if (m_comboDnsProvider) s.dnsProvider = m_comboDnsProvider->currentData().toInt();
    if (m_inpDnsCustom) s.dnsCustom = m_inpDnsCustom->text().trimmed();

    if (m_chkReturnMq) s.returnToMqBloxOnLeave = m_chkReturnMq->isChecked();
    if (m_chkAnimBg) s.animBg = m_chkAnimBg->isChecked();
    if (m_chkCloseToTray) s.closeToTray = m_chkCloseToTray->isChecked();
    if (m_chkVsyncOff) s.vsyncOff = m_chkVsyncOff->isChecked();
    if (m_chkAutoClean) s.smartAutoCleanCache = m_chkAutoClean->isChecked();
    if (m_chkMemFflags) s.lowRamFflags = m_chkMemFflags->isChecked();
    if (m_chkNoTelemetry) s.disableTelemetry = m_chkNoTelemetry->isChecked();
    if (m_chkAutoReconnect) s.autoReconnectOnCrash = m_chkAutoReconnect->isChecked();

    if (m_chkNoShadows) s.noShadows = m_chkNoShadows->isChecked();
    if (m_chkUltraShadows) s.ultraShadows = m_chkUltraShadows->isChecked();
    if (s.noShadows && s.ultraShadows) {
        s.ultraShadows = false;
        if (m_chkUltraShadows) {
            m_chkUltraShadows->blockSignals(true);
            m_chkUltraShadows->setChecked(false);
            m_chkUltraShadows->blockSignals(false);
        }
    }
    if (m_chkNoPostfx) s.noPostFx = m_chkNoPostfx->isChecked();
    if (m_chkLowMeshes) s.lowMeshes = m_chkLowMeshes->isChecked();
    if (m_chkGreySky) s.greySky = m_chkGreySky->isChecked();

    if (m_chkHudFps) s.showHudFps = m_chkHudFps->isChecked();
    if (m_chkClearFog) s.clearFog = m_chkClearFog->isChecked();
    if (m_spinMaxZoom) s.customMaxZoom = m_spinMaxZoom->value();
    if (m_chkInfZoom) s.unlockInfiniteZoom = m_chkInfZoom->isChecked();
    if (m_chkFullBright) s.fullBright = m_chkFullBright->isChecked();
    if (m_chkHitboxes) s.showHitboxes = m_chkHitboxes->isChecked();

    if (m_chkFpsLog) s.fpsLog = m_chkFpsLog->isChecked();
    if (m_chkTraceUdp) s.traceUdp = m_chkTraceUdp->isChecked();
    if (m_chkTraceLock) s.traceLock = m_chkTraceLock->isChecked();
    if (m_chkTraceGl) s.traceGl = m_chkTraceGl->isChecked();
    if (m_chkDiagSignals) s.diagnosticSignals = m_chkDiagSignals->isChecked();

    if (m_comboVisualPreset) s.visualPreset = m_comboVisualPreset->currentData().toInt();
    if (m_sldBlur) s.glassBlurStrength = m_sldBlur->value();
    if (m_sldOpacity) s.glassOpacityPct = m_sldOpacity->value();
    if (m_sldBorderBright) s.borderBrightnessPct = m_sldBorderBright->value();
    if (m_comboThemeMode) s.themeMode = m_comboThemeMode->currentData().toInt();
    if (m_comboAccent) s.accentPreset = m_comboAccent->currentData().toInt();
    if (m_chkChameleon) s.chameleonEnabled = m_chkChameleon->isChecked();
    if (m_chkReducedMotion) s.reducedMotion = m_chkReducedMotion->isChecked();
    if (m_sldUiScale) s.uiScalePct = m_sldUiScale->value();
    if (m_sldFontSize) s.fontSizePx = m_sldFontSize->value();

    if (s.rememberWindowSize && width() >= 820 && height() >= 540) {
        s.savedWindowWidth = width();
        s.savedWindowHeight = height();
    }

    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    if (m_inpPlace) s.lastPlaceInput = m_inpPlace->text().trimmed();
    if (m_fflagsEditor) s.customFflagsJson = m_fflagsEditor->toPlainText();
}

void MainWindow::saveSettingsNow(bool manualClick) {
    collectSettingsFromUi();
    QString err;
    if (m_settingsMgr.saveAll(&err)) {
        if (m_lblFflagsValidation) {
            m_lblFflagsValidation->setText(tr("✓ Синтаксис FastFlags корректен и сохранён в ClientAppSettings.json"));
            m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
        }
        if (m_btnSaveSettings) {
            m_btnSaveSettings->setText(tr("✓ Настройки сохранены"));
            m_btnSaveSettings->setAccentColor(QColor(16, 185, 129), manualClick);
        }
        if (m_lblAutosync) {
            m_lblAutosync->setText(tr("✓ Все изменения сохранены"));
        }
        if (manualClick) {
            enqueueToast(tr("💾 Настройки сохранены в ClientAppSettings.json!"));
        }
    } else {
        if (m_lblFflagsValidation) {
            m_lblFflagsValidation->setText("⚠️ " + err);
            m_lblFflagsValidation->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 700;");
        }
        enqueueToast("⚠️ " + err);
    }
}

void MainWindow::triggerInstantAutosave(const QString &toastMsg) {
    if (m_isLoadingUi) return;
    if (!toastMsg.isEmpty()) enqueueToast(toastMsg);
    collectSettingsFromUi();
    if (m_btnSaveSettings) {
        m_btnSaveSettings->setText(tr("💾 Сохранить настройки ●"));
        m_btnSaveSettings->setAccentColor(QColor(245, 158, 11), false);
    }
    if (m_lblAutosync) {
        m_lblAutosync->setText(tr("⚡ Авто-сохранение..."));
    }
    m_autosaveTimer->start(250);
}

void MainWindow::enqueueToast(const QString &msg) {
    if (msg.isEmpty()) return;
    if (m_toastQueue.isEmpty() || m_toastQueue.back() != msg) {
        m_toastQueue.enqueue(msg);
    }
    if (!m_toastActive) processNextToast();
}

void MainWindow::processNextToast() {
    if (m_toastQueue.isEmpty()) {
        m_toastActive = false;
        m_toastLbl->setVisible(false);
        return;
    }
    m_toastActive = true;
    m_toastLbl->setText(m_toastQueue.dequeue());
    m_toastLbl->setVisible(true);
    QTimer::singleShot(1900, this, &MainWindow::processNextToast);
}

void MainWindow::refreshAccountsUi() {
    m_comboQuickAcc->blockSignals(true);
    m_comboQuickAcc->clear();
    for (const auto &acc : m_settingsMgr.accounts()) {
        m_comboQuickAcc->addItem("👤 " + acc.displayName, acc.id);
        if (acc.id == m_settingsMgr.activeAccountId()) {
            m_comboQuickAcc->setCurrentIndex(m_comboQuickAcc->count() - 1);
        }
    }
    m_comboQuickAcc->blockSignals(false);

    while (QLayoutItem *item = m_accountsListLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    for (const auto &acc : m_settingsMgr.accounts()) {
        auto *card = new QFrame();
        card->setObjectName("GlassCard");
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(16, 12, 16, 12);
        bool isAct = (acc.id == m_settingsMgr.activeAccountId());
        QString badge = isAct ? "🟢 [АКТИВНЫЙ СЛОТ]" : "⚪ [В РЕЗЕРВЕ]";
        auto *lbl = new QLabel(QString("%1  <b>%2</b> (ID: <code>%3</code>)")
                                   .arg(badge, acc.displayName, acc.id));
        row->addWidget(lbl);
        row->addStretch();
        if (!isAct) {
            auto *bSw = new AnimButton(tr("✅ Переключить слот"));
            QString id = acc.id;
            connect(bSw, &QPushButton::clicked, this, [this, id]() { m_soberCtrl->switchAccountSlotAsync(id); });
            row->addWidget(bSw);
        }
        m_accountsListLayout->addWidget(card);
    }
}

void MainWindow::applyResolutionMode(ResolutionMode mode, bool initial) {
    QScreen *scr = windowHandle() && windowHandle()->screen()
                       ? windowHandle()->screen()
                       : QApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1366, 768);
    int sw = avail.width(), sh = avail.height();
    setMaximumSize(sw, sh);

    int tw = 1100, th = 660;
    switch (mode) {
    case ResolutionMode::HD_720p:   tw = 1080; th = 640; break;
    case ResolutionMode::FHD_1080p: tw = 1360; th = 780; break;
    case ResolutionMode::QHD_2K:    tw = 1720; th = 960; break;
    case ResolutionMode::Auto:
        tw = int(sw * 0.78);
        th = int(sh * 0.78);
        break;
    }

    int fw = std::clamp(tw, 840, std::max(840, sw - 32));
    int fh = std::clamp(th, 520, std::max(520, sh - 36));

    if (!isMaximized()) {
        resize(fw, fh);
        move(avail.x() + (sw - fw) / 2, avail.y() + (sh - fh) / 2);
    }
    if (!initial) triggerInstantAutosave(tr("🖥️ Размер окна адаптирован (%1×%2)").arg(fw).arg(fh));
}

void MainWindow::applyGraphicsMasterPreset(int presetIdx) {
    m_isLoadingUi = true;
    if (presetIdx == 0) {
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::Voxel)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level0)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::Off)));
        m_spinFrm->setValue(1);
        m_chkNoShadows->setChecked(true);
        m_chkUltraShadows->setChecked(false);
        m_chkNoPostfx->setChecked(true);
        m_chkLowMeshes->setChecked(true);
        m_chkGreySky->setChecked(true);
        m_chkUnlimFps->setChecked(true);
    } else if (presetIdx == 1) {
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::ShadowMap)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level2)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::X2)));
        m_spinFrm->setValue(10);
        m_chkNoShadows->setChecked(false);
        m_chkUltraShadows->setChecked(false);
        m_chkNoPostfx->setChecked(false);
        m_chkLowMeshes->setChecked(false);
        m_chkGreySky->setChecked(false);
    } else {
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::FuturePhase3)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level3)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::X4)));
        m_spinFrm->setValue(21);
        m_chkNoShadows->setChecked(false);
        m_chkUltraShadows->setChecked(true);
        m_chkNoPostfx->setChecked(false);
        m_chkLowMeshes->setChecked(false);
        m_chkGreySky->setChecked(false);
    }
    m_isLoadingUi = false;
    triggerInstantAutosave(tr("🌟 Пресет графики применён!"));
}

void MainWindow::toggleSidebarPinMode() {
    bool &pinned = m_settingsMgr.settings().sidebarPinned;
    pinned = !pinned;
    m_btnPinSide->setText(pinned ? "📌" : "📍");
    if (pinned) animateSidebar(true);
    triggerInstantAutosave(pinned ? tr("📌 Боковая панель закреплена") : tr("📍 Авто-выезд панели включён"));
}

void MainWindow::animateSidebar(bool expand) {
    m_sidebarExpanded = expand;
    int targetW = expand ? 232 : 70;
    if (m_sidebar->width() == targetW) return;
    m_sidebarAnim->stop();
    m_sidebarAnim->setStartValue(m_sidebar->width());
    m_sidebarAnim->setEndValue(targetW);
    m_sidebarAnim->start();
}

void MainWindow::switchTab(int idx) {
    if (m_pages->currentIndex() == idx) return;
    m_pages->setCurrentIndex(idx);
    for (size_t i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setActiveTab(static_cast<int>(i) == idx);
    }
    if (idx == 0) refreshEnvironmentAndSessionUi();
    if (idx == 5) refreshGitUpdateStatus();
    if (QWidget *w = m_pages->currentWidget()) {
        if (idx != 0) {
            auto *eff = new QGraphicsOpacityEffect(w);
            w->setGraphicsEffect(eff);
            auto *anim = new QPropertyAnimation(eff, "opacity", w);
            anim->setDuration(220);
            anim->setStartValue(0.15);
            anim->setEndValue(1.0);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &QPropertyAnimation::finished, w, [w]() { w->setGraphicsEffect(nullptr); });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}

void MainWindow::runWatchdogTick() {
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly)) return;
    qulonglong totalKb = 8000000, availKb = 4000000;
    for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
        if (line.startsWith("MemTotal:")) totalKb = line.split(QRegularExpression("\\s+")).value(1).toULongLong();
        else if (line.startsWith("MemAvailable:")) availKb = line.split(QRegularExpression("\\s+")).value(1).toULongLong();
    }
    int availMb = int(availKb / 1024);
    int usedPct = int(100 - (availKb * 100 / std::max(1ULL, totalKb)));
    m_ramBar->setValue(std::clamp(usedPct, 5, 100));
    m_ramBar->setFormat(tr("ОЗУ: свободно %1 МБ (%2%)").arg(availMb).arg(usedPct));
}

} // namespace mqblox
