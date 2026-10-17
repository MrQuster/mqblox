#include "core/SettingsRegistry.hpp"
#include <QClipboard>
#include <QScrollBar>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "ui/MainWindow.hpp"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QNetworkCookie>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSizeGrip>
#include <QWebEngineCookieStore>
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
    // Сбрасываем минимальную ширину у всех вложенных виджетов, чтобы длинный текст никогда не распирал карточку шире окна
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
    m_apiClient = new RobloxApiClient(this);

    setWindowTitle(tr("MqBlox v34.6 [HyperOS 4 Liquid Glass]"));
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
            m_lblFflagsValidation->setText(tr("✓ Синтаксис FastFlags корректен и сохранён"));
            m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
        } else {
            m_lblFflagsValidation->setText("⚠️ " + err);
            m_lblFflagsValidation->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 700;");
            enqueueToast("⚠️ " + err);
        }
    });

    initUi();
    initTrayAndShortcuts();
    syncUiFromSettings();
    refreshAccountsUi();
    setupWebProfile(m_settingsMgr.activeAccountId());
    applyResolutionMode(m_settingsMgr.settings().resMode, true);
    m_isLoadingUi = false;

    connect(m_soberCtrl, &SoberController::hardwareDetected, this, [this]() {
        m_hwBannerLbl->setText(tr("🔍 Железо: <b>%1</b> (%2 потоков) • GPU: <b>%3</b>")
                                   .arg(m_soberCtrl->cpuModel())
                                   .arg(m_soberCtrl->cpuThreads())
                                   .arg(m_soberCtrl->gpuModel()));
    });
    connect(m_soberCtrl, &SoberController::logGenerated, this, [this](const QString &line) {
        m_logView->append(line);
    });
    connect(m_soberCtrl, &SoberController::gameStarted, this, [this]() {
        m_statusBadge->setText(m_sidebarExpanded ? tr("● В ИГРЕ") : "🎮");
        if (m_settingsMgr.settings().pauseWebInGame && m_webPage) {
            m_webPage->setLifecycleState(QWebEnginePage::LifecycleState::Frozen);
        }
        enqueueToast(tr("🚀 Клиент Roblox запущен (Микрофон управляется самим Sober)"));
    });
    connect(m_soberCtrl, &SoberController::gameLeftByPlayer, this, [this]() {
        if (m_webPage) m_webPage->setLifecycleState(QWebEnginePage::LifecycleState::Active);
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        showNormal(); raise(); activateWindow();
        switchTab(0);
        enqueueToast(tr("🏠 Выход из плейса: возврат на Главную MqBlox"));
    });
    connect(m_soberCtrl, &SoberController::gameFinished, this, [this](int, bool crashed) {
        if (m_webPage) m_webPage->setLifecycleState(QWebEnginePage::LifecycleState::Active);
        m_statusBadge->setText(m_sidebarExpanded ? tr("● В МЕНЮ MQBLOX") : "●");
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        showNormal(); raise(); activateWindow();
        if (crashed && m_settingsMgr.settings().autoReconnectOnCrash) {
            enqueueToast(tr("⚠️ Обнаружен сбой клиента! Авто-перезаход через 3 с..."));
            QTimer::singleShot(3000, this, [this]() { m_soberCtrl->launchRoblox(); });
        }
    });
    connect(m_soberCtrl, &SoberController::accountSwitchCompleted, this, [this](const QString &accId) {
        refreshAccountsUi();
        setupWebProfile(accId);
        syncActiveProfile(true);
        enqueueToast(tr("👤 Аккаунт переключён на «%1»").arg(accId));
    });

    connect(m_apiClient, &RobloxApiClient::profileUpdated, this,
            [this](qulonglong uid, const QString &uname, const QString &disp) {
                m_lblSkinDisp->setText("👤 " + disp);
                if (auto *acc = m_settingsMgr.activeAccount()) {
                    acc->userId = uid;
                    acc->username = uname;
                    acc->displayName = disp;
                    m_settingsMgr.saveAll();
                    refreshAccountsUi();
                }
            });
    connect(m_apiClient, &RobloxApiClient::robuxUpdated, this, [this](const QString &rbx) {
        m_lblRobux->setText("◈ R$ " + rbx);
        if (auto *acc = m_settingsMgr.activeAccount()) {
            acc->robux = rbx;
            m_settingsMgr.saveAll();
            refreshAccountsUi();
        }
    });

    m_soberCtrl->detectHardwareAsync();
    syncActiveProfile(false);

    auto *watchdog = new QTimer(this);
    connect(watchdog, &QTimer::timeout, this, [this]() { runWatchdogTick(); });
    watchdog->start(3000);
    runWatchdogTick();

    if (!m_launchUri.isEmpty() && m_settingsMgr.settings().autoLaunchFromBrowser) {
        QTimer::singleShot(400, this, [this]() { m_soberCtrl->launchRoblox(m_launchUri); });
    }
}

MainWindow::~MainWindow() {
    if (m_webView) m_webView->setPage(nullptr);
    delete m_webPage;
    delete m_webProfile;
}

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
        m_trayIcon->setToolTip(tr("MqBlox v34.6 — HyperOS 4 Liquid Glass"));

        auto *menu = new QMenu(nullptr);
        menu->setStyleSheet(
            "QMenu { background-color: #070A14; color: #F8FAFC; border: 1px solid #38BDF8; border-radius: 10px; padding: 6px; font-weight: 700; }"
            "QMenu::item { padding: 7px 18px; border-radius: 6px; }"
            "QMenu::item:selected { background-color: #38BDF8; color: #030408; }"
        );
        menu->addAction(tr("🌌 Развернуть MqBlox"), this, [this]() { showNormal(); raise(); activateWindow(); });
        menu->addAction(tr("▶ Запустить Roblox"), this, [this]() { m_soberCtrl->launchRoblox(); });
        menu->addAction(tr("🛑 Закрыть Roblox"), this, [this]() { m_soberCtrl->stopRobloxAsync(); });
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
                else { showNormal(); raise(); activateWindow(); }
            }
        });
        m_trayIcon->show();
    }

    new QShortcut(QKeySequence(Qt::Key_F5), this, [this]() { syncActiveProfile(true); });
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
    auto *logoLbl = new QLabel("✦ MqBlox");
    logoLbl->setObjectName("LogoText");
    m_pinHintLbl = new QLabel("💧 HyperOS 4 Glass");
    m_pinHintLbl->setStyleSheet("font-size: 10px; font-weight: 800; color: #38BDF8;");
    tCol->addWidget(logoLbl);
    tCol->addWidget(m_pinHintLbl);
    hdrRow->addWidget(m_titleColWidget);
    hdrRow->addStretch();
    sideLay->addLayout(hdrRow);

    struct NavItem { QString ico; QString title; int idx; };
    const std::vector<NavItem> navItems = {
        {"🏠", tr("Главная"), 0},
        {"👥", tr("Аккаунты"), 1},
        {"⚙️", tr("Система"), 2},
        {"✨", tr("Графика"), 3},
        {"🔥", tr("Твики и Моды"), 4},
        {"🔄", tr("Обновления"), 5}
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
    auto *lblAuthT = new QLabel(tr("👑 СОЗДАТЕЛИ ПРОЕКТА:"));
    lblAuthT->setStyleSheet("font-size: 10px; font-weight: 900; color: #FDE047;");
    auto *lblAuthN = new QLabel("✦ MrQuster\n✦ Gemini 4 Argon");
    lblAuthN->setStyleSheet("font-size: 12px; font-weight: 900; color: #FFFFFF;");
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
    auto *topHint = new QLabel(tr("MqBlox v34.6 • HyperOS 4 Liquid Glass • Zero-Lag"));
    topHint->setObjectName("SubText");
    topBar->addWidget(topHint);

    m_toastLbl = new QLabel();
    m_toastLbl->setObjectName("ToastBanner");
    m_toastLbl->setVisible(false);
    topBar->addWidget(m_toastLbl);
    topBar->addStretch();

    topBar->addWidget(new QLabel(tr("Экран:")));
    m_comboRes = new DarkComboBox();
    m_comboRes->setMinimumWidth(168);
    m_comboRes->addItem(tr("Авто (Под монитор)"), static_cast<int>(ResolutionMode::Auto));
    m_comboRes->addItem(tr("HD (1200×680)"), static_cast<int>(ResolutionMode::HD_720p));
    m_comboRes->addItem(tr("Full HD (1480×840)"), static_cast<int>(ResolutionMode::FHD_1080p));
    m_comboRes->addItem(tr("2K QHD (1920×1080)"), static_cast<int>(ResolutionMode::QHD_2K));
    connect(m_comboRes, &QComboBox::currentIndexChanged, this, [this](int idx) {
        auto mode = static_cast<ResolutionMode>(m_comboRes->itemData(idx).toInt());
        applyResolutionMode(mode, false);
    });
    topBar->addWidget(m_comboRes);

    topBar->addWidget(new QLabel(tr("Профиль:")));
    m_comboQuickAcc = new DarkComboBox();
    m_comboQuickAcc->setMinimumWidth(145);
    connect(m_comboQuickAcc, &QComboBox::currentIndexChanged, this, [this](int i) {
        QString id = m_comboQuickAcc->itemData(i).toString();
        if (!id.isEmpty() && id != m_settingsMgr.activeAccountId()) {
            m_soberCtrl->switchAccountSlotAsync(id);
        }
    });
    topBar->addWidget(m_comboQuickAcc);

    auto *btnMin = new AnimButton("—"); btnMin->setFixedWidth(34);
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    m_btnMax = new AnimButton("□"); m_btnMax->setFixedWidth(34);
    connect(m_btnMax, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) { showNormal(); m_btnMax->setText("□"); }
        else { showMaximized(); m_btnMax->setText("❐"); }
    });
    auto *btnClose = new AnimButton("✕", nullptr, QColor(239, 68, 68), false, true);
    btnClose->setFixedWidth(34);
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    topBar->addWidget(btnMin);
    topBar->addWidget(m_btnMax);
    topBar->addWidget(btnClose);
    rightLay->addLayout(topBar);

    m_pages = new QStackedWidget();
    m_pages->addWidget(buildHomeRobloxPage());
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
    m_btnSaveSettings->setToolTip(tr("Сохранить и применить все изменения настроек к клиенту Sober (Ctrl+S)"));
    connect(m_btnSaveSettings, &QPushButton::clicked, this, [this]() { saveSettingsNow(true); });
    new QShortcut(QKeySequence("Ctrl+S"), this, [this]() { saveSettingsNow(true); });

    auto *btnRejoin = new AnimButton(tr("🔄 Перезайти"));
    connect(btnRejoin, &QPushButton::clicked, this, [this]() { saveSettingsNow(false); m_soberCtrl->launchRoblox(); });

    auto *btnKill = new AnimButton(tr("🛑 Выйти из плейса"), nullptr, QColor(239, 68, 68), false, true);
    connect(btnKill, &QPushButton::clicked, this, [this]() { m_soberCtrl->stopRobloxAsync(); });

    auto *btnPlay = new AnimButton(tr("▶  ЗАПУСТИТЬ ИГРУ"), nullptr, QColor(16, 185, 129), true, false);
    connect(btnPlay, &QPushButton::clicked, this, [this]() { saveSettingsNow(false); m_soberCtrl->launchRoblox(); });

    bottomBar->addWidget(m_lblAutosync);
    bottomBar->addWidget(m_btnSaveSettings);
    auto *btnResetCat = new AnimButton(tr("↺ Сбросить вкладку"));
    btnResetCat->setToolTip(tr("Сбросить параметры текущей категории к безопасным значениям по умолчанию"));
    connect(btnResetCat, &QPushButton::clicked, this, [this]() { resetCurrentCategoryToDefaults(); });
    bottomBar->addWidget(btnResetCat);
    auto *btnResetLook = new AnimButton(tr("🎨 Сбросить вид"), nullptr, QColor(56, 189, 248));
    btnResetLook->setToolTip(tr("Мгновенно вернуть стандартное читаемое оформление Liquid Glass"));
    connect(btnResetLook, &QPushButton::clicked, this, [this]() { resetAppearanceToDefault(); });
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
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *rightCard = new QFrame();
    rightCard->setObjectName("GlassCard");
    auto *rrLay = new QVBoxLayout(rightCard);
    rrLay->setContentsMargins(12, 12, 12, 12);
    rrLay->setSpacing(8);

    auto *webBar = new QHBoxLayout();
    webBar->setSpacing(6);

    auto *btnBack = new AnimButton("◀"); btnBack->setFixedWidth(34);
    auto *btnFwd  = new AnimButton("▶"); btnFwd->setFixedWidth(34);
    auto *btnRel  = new AnimButton("🔄"); btnRel->setFixedWidth(36);

    auto *bHome   = new AnimButton(tr("🏠 Главная"));
    auto *bCharts = new AnimButton(tr("🔥 Плейсы"));
    auto *bAvatar = new AnimButton(tr("👕 Аватар"));

    m_inpPlace = new QLineEdit();
    m_inpPlace->setMinimumWidth(140);
    m_inpPlace->setPlaceholderText(tr("⚡ Place ID или ссылка на VIP-сервер..."));
    auto launchByInput = [this]() {
        QString uri = SettingsManager::parsePlaceUri(m_inpPlace->text());
        if (uri.isEmpty()) {
            enqueueToast(tr("⚠️ Введи корректный Place ID или ссылку на игру"));
            return;
        }
        m_soberCtrl->launchRoblox(uri);
    };
    connect(m_inpPlace, &QLineEdit::returnPressed, this, launchByInput);
    auto *btnGoId = new AnimButton(tr("🚀 Войти"), nullptr, QColor(16, 185, 129), true);
    connect(btnGoId, &QPushButton::clicked, this, launchByInput);

    m_lblSkinDisp = new QLabel("👤 MrQuster");
    m_lblSkinDisp->setStyleSheet("font-size: 12px; font-weight: 900; color: #FDE047; padding-left: 4px;");
    m_lblSkinUser = new QLabel();
    m_lblSkinUser->setVisible(false);

    m_lblRobux = new QLabel("◈ R$ —");
    m_lblRobux->setObjectName("RobuxCardBadge");
    m_lblRobux->setAlignment(Qt::AlignCenter);

    auto *btnSync = new AnimButton("🔄");
    btnSync->setFixedWidth(34);
    connect(btnSync, &QPushButton::clicked, this, [this]() { syncActiveProfile(true); });

    webBar->addWidget(btnBack);
    webBar->addWidget(btnFwd);
    webBar->addWidget(btnRel);
    webBar->addWidget(bHome);
    webBar->addWidget(bCharts);
    webBar->addWidget(bAvatar);
    webBar->addWidget(m_inpPlace, 1);
    webBar->addWidget(btnGoId);
    webBar->addWidget(m_lblSkinDisp);
    webBar->addWidget(m_lblRobux);
    webBar->addWidget(btnSync);
    rrLay->addLayout(webBar);

    m_webView = new QWebEngineView();
    m_webView->setStyleSheet("background: #030408;");
    connect(btnBack, &QPushButton::clicked, m_webView, &QWebEngineView::back);
    connect(btnFwd,  &QPushButton::clicked, m_webView, &QWebEngineView::forward);
    connect(btnRel,  &QPushButton::clicked, m_webView, &QWebEngineView::reload);
    connect(bHome,   &QPushButton::clicked, this, [this]() { m_webView->setUrl(QUrl("https://www.roblox.com/home")); });
    connect(bCharts, &QPushButton::clicked, this, [this]() { m_webView->setUrl(QUrl("https://www.roblox.com/charts")); });
    connect(bAvatar, &QPushButton::clicked, this, [this]() { m_webView->setUrl(QUrl("https://www.roblox.com/my/avatar")); });
    connect(m_webView, &QWebEngineView::loadFinished, this, &MainWindow::onWebPageLoaded);
    rrLay->addWidget(m_webView, 1);

    lay->addWidget(rightCard, 1);
    return page;
}

QWidget *MainWindow::buildAccountsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);
    auto *cardTop = new QFrame();
    cardTop->setObjectName("GlassCard");
    auto *ct = new QVBoxLayout(cardTop);
    ct->setContentsMargins(18, 16, 18, 16);
    ct->setSpacing(10);

    auto *t1 = new QLabel(tr("👥 Мульти-Аккаунт Центр (Безопасное хранилище SHA-256 Vault)"));
    t1->setObjectName("SectionTitle");
    ct->addWidget(t1);

    auto *btnRow = new QHBoxLayout();
    auto *bAdd = new AnimButton(tr("➕ Добавить аккаунт"));
    auto *bSave = new AnimButton(tr("📥 Сохранить текущий вход в слот"));
    auto *bTok = new AnimButton(tr("🔑 Привязать .ROBLOSECURITY"));

    connect(bAdd, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Новый аккаунт"), tr("Название слота:"), QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            AccountProfile p;
            p.id = QString("acc_%1").arg(QDateTime::currentSecsSinceEpoch() % 100000);
            p.label = name.trimmed();
            p.displayName = name.trimmed();
            m_settingsMgr.accounts().push_back(p);
            m_soberCtrl->switchAccountSlotAsync(p.id);
        }
    });
    connect(bSave, &QPushButton::clicked, this, [this]() {
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        enqueueToast(tr("✓ Игровая сессия сохранена в слот"));
    });
    connect(bTok, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString ck = QInputDialog::getText(this, tr("Безопасное хранилище"),
                                           tr("Вставь .ROBLOSECURITY (сохраняется в зашифрованный vault 0600):"),
                                           QLineEdit::Password, "", &ok);
        if (ok && !ck.trimmed().isEmpty()) {
            SecretStore::storeToken(m_settingsMgr.activeAccountId(), ck.trimmed());
            syncActiveProfile(true);
        }
    });

    btnRow->addWidget(bAdd); btnRow->addWidget(bSave); btnRow->addWidget(bTok);
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

    auto *cardHw = new QFrame();
    cardHw->setObjectName("GlassCard");
    auto *hw = new QGridLayout(cardHw);
    hw->setContentsMargins(18, 16, 18, 16);
    hw->setSpacing(12);
    hw->setColumnStretch(0, 0);
    hw->setColumnStretch(1, 1);

    auto *thw = new QLabel(tr("🖥️ Видеокарта и Процессор"));
    thw->setObjectName("SectionTitle");
    hw->addWidget(thw, 0, 0, 1, 2);

    m_hwBannerLbl = new QLabel(tr("🔍 Определение оборудования..."));
    m_hwBannerLbl->setObjectName("SubText");
    hw->addWidget(m_hwBannerLbl, 1, 0, 1, 2);

    hw->addWidget(new QLabel(tr("🎮 Профиль драйвера Видеокарты (GPU):")), 2, 0);
    m_comboGpuVendor = new DarkComboBox();
    m_comboGpuVendor->addItem(tr("Авто-определение"), static_cast<int>(GpuVendorMode::Auto));
    m_comboGpuVendor->addItem(tr("🔴 AMD Radeon (Mesa RADV / RadeonSI + GPL + 4GB Cache)"), static_cast<int>(GpuVendorMode::AMD));
    m_comboGpuVendor->addItem(tr("🟢 NVIDIA GeForce (Threaded Opt + NV Disk Cache)"), static_cast<int>(GpuVendorMode::Nvidia));
    m_comboGpuVendor->addItem(tr("🔵 Intel Arc / UHD Graphics (Mesa ANV + GPL)"), static_cast<int>(GpuVendorMode::Intel));
    hw->addWidget(m_comboGpuVendor, 2, 1);

    hw->addWidget(new QLabel(tr("⚙️ Архитектура Процессора (CPU):")), 3, 0);
    m_comboCpuVendor = new DarkComboBox();
    m_comboCpuVendor->addItem(tr("Авто-оптимизация потоков"), static_cast<int>(CpuVendorMode::Auto));
    m_comboCpuVendor->addItem(tr("🔵 Intel Core i3/i5/i7/i9"), static_cast<int>(CpuVendorMode::Intel));
    m_comboCpuVendor->addItem(tr("🟠 AMD Ryzen / FX"), static_cast<int>(CpuVendorMode::AMD));
    hw->addWidget(m_comboCpuVendor, 3, 1);
    lay->addWidget(cardHw);

    // Скрытые заглушки, чтобы не ломать сериализацию: микрофон теперь выбирает сам Sober в настройках игры!
    m_comboAudioOut = new DarkComboBox(page);
    m_comboAudioOut->setVisible(false);
    m_comboAudioIn = new DarkComboBox(page);
    m_comboAudioIn->setVisible(false);

    auto *cardDrv = new QFrame();
    cardDrv->setObjectName("GlassCard");
    auto *c1 = new QVBoxLayout(cardDrv);
    c1->setContentsMargins(18, 16, 18, 16);
    c1->setSpacing(10);

    auto *t1 = new QLabel(tr("⚙️ Параметры Системы и Оформления"));
    t1->setObjectName("SectionTitle");
    c1->addWidget(t1);

    auto *micNote = new QLabel(tr("🎙️ <b>Микрофон и наушники выбираются напрямую самим Sober</b> внутри игры Roblox (Esc → Settings → Input / Output Device)."));
    micNote->setObjectName("SubText");
    micNote->setWordWrap(true);
    c1->addWidget(micNote);

    auto *rowWin = new QHBoxLayout();
    rowWin->setSpacing(10);
    rowWin->addWidget(new QLabel(tr("Оконный протокол:")));
    m_comboWinDrv = new DarkComboBox();
    m_comboWinDrv->addItem(tr("Автоматически (Wayland + X11 Passthrough)"), static_cast<int>(WindowDriverMode::AutoPassthrough));
    m_comboWinDrv->addItem(tr("X11 / XWayland"), static_cast<int>(WindowDriverMode::ForceX11));
    m_comboWinDrv->addItem(tr("Wayland Native"), static_cast<int>(WindowDriverMode::ForceWayland));
    rowWin->addWidget(m_comboWinDrv, 1);
    c1->addLayout(rowWin);

    auto *rowBg = new QHBoxLayout();
    rowBg->setSpacing(10);
    rowBg->addWidget(new QLabel(tr("🌌 Задний фон лаунчера:")));
    auto *btnCustomBg = new AnimButton(tr("📂 Выбрать своё фото или видео (.png/.jpg/.mp4)"), nullptr, QColor(56, 189, 248));
    connect(btnCustomBg, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Выбрать фон"), QDir::homePath(),
                                                    tr("Файлы фона (*.png *.jpg *.jpeg *.mp4 *.webm *.mkv)"));
        if (!path.isEmpty()) {
            m_rootCosmos->loadCustomVideoFile(path);
            enqueueToast(tr("🌌 Фон успешно обновлён и кэширован (0% нагрузки на CPU)!"));
        }
    });
    rowBg->addWidget(btnCustomBg, 1);
    c1->addLayout(rowBg);

    m_chkReturnMq      = new QCheckBox(tr("При выходе из плейса (Leave Game) возвращать на Главную MqBlox"));
    m_chkPauseWeb      = new QCheckBox(tr("Замораживать браузер во время игры (0% нагрузки в фоне)"));
    m_chkAnimBg        = new QCheckBox(tr("Ультра-быстрый кэш преломления HyperOS 4 Liquid Glass"));
    m_chkCloseToTray   = new QCheckBox(tr("Сворачивать MqBlox в системный трей при закрытии окна (✕)"));
    m_chkGlThread      = new QCheckBox(tr("Многопоточная обработка команд видеокарты (glthread / Threaded Opt)"));
    m_chkAsyncShaders  = new QCheckBox(tr("Асинхронная компиляция шейдеров (RADV GPL / radeonsi_sync_compile=false)"));
    m_chkVsyncOff      = new QCheckBox(tr("Отключить VSync компоновщика (vblank_mode=0 + Vulkan Immediate)"));
    m_chkPwBuffer      = new QCheckBox(tr("Низкая задержка звука PipeWire (1024/48000)"));
    m_chkMallocTrim    = new QCheckBox(tr("Принудительный возврат ОЗУ ядру Linux (MALLOC_TRIM_THRESHOLD_=65536)"));
    m_chkAutoClean     = new QCheckBox(tr("Умная авто-очистка кэша карт (>500 МБ)"));
    m_chkMemFflags     = new QCheckBox(tr("Быстрая выгрузка неиспользуемых мешей из ОЗУ (Low-RAM FFlags)"));
    m_chkNoTelemetry   = new QCheckBox(tr("Отключить фоновую телеметрию Roblox"));
    m_chkGamemode      = new QCheckBox(tr("Приоритет процесса через Feral GameMode"));
    m_chkAutoLaunch    = new QCheckBox(tr("Мгновенно запускать игру при клике Play"));
    m_chkAutoReconnect = new QCheckBox(tr("Авто-перезаход при аварийном вылете клиента"));
    m_chkHideSober     = new QCheckBox(tr("Скрывать стандартный ярлык Sober из меню приложений"));

    for (QCheckBox *chk : {
             m_chkReturnMq, m_chkPauseWeb, m_chkAnimBg, m_chkCloseToTray, m_chkGlThread,
             m_chkAsyncShaders, m_chkVsyncOff, m_chkPwBuffer, m_chkMallocTrim, m_chkAutoClean,
             m_chkMemFflags, m_chkNoTelemetry, m_chkGamemode, m_chkAutoLaunch, m_chkAutoReconnect, m_chkHideSober}) {
        c1->addWidget(chk);
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    for (QComboBox *cmb : {m_comboGpuVendor, m_comboCpuVendor, m_comboWinDrv}) {
        connect(cmb, &QComboBox::currentIndexChanged, this, [this]() { triggerInstantAutosave(); });
    }

    lay->addWidget(cardDrv);
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
    auto *wLbl = new QLabel(tr("⚠️ <b>Предупреждение:</b> Изменение клиентских FastFlags выполняется на ваш риск."));
    wLbl->setWordWrap(true);
    wLay->addWidget(wLbl);
    lay->addWidget(warnFrame);

    auto *cardPr = new QFrame();
    cardPr->setObjectName("GlassCard");
    auto *cp = new QVBoxLayout(cardPr);
    cp->setContentsMargins(18, 16, 18, 16);
    cp->setSpacing(10);

    auto *tp = new QLabel(tr("🌟 Глобальные пресеты графики + Импорт / Экспорт профилей"));
    tp->setObjectName("SectionTitle");
    cp->addWidget(tp);

    auto *rowBtns = new QHBoxLayout();
    auto *bFps = new AnimButton(tr("🚀 УЛЬТРА FPS"), nullptr, QColor(16, 185, 129));
    auto *bBal = new AnimButton(tr("⚖️ БАЛАНС"), nullptr, QColor(56, 189, 248));
    auto *bUlt = new AnimButton(tr("💎 УЛЬТРА РЕАЛИЗМ"), nullptr, QColor(168, 85, 247));
    connect(bFps, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(0); });
    connect(bBal, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(1); });
    connect(bUlt, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(2); });
    rowBtns->addWidget(bFps); rowBtns->addWidget(bBal); rowBtns->addWidget(bUlt);
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
                enqueueToast(tr("📥 Профиль успешно импортирован!"));
            } else {
                enqueueToast("⚠️ " + err);
            }
        }
    });
    rowIo->addWidget(bExp); rowIo->addWidget(bImp);
    cp->addLayout(rowIo);

    m_lblPresetDesc = new QLabel(tr("Выбери готовый профиль или настрой параметры вручную ниже."));
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

    g1->addWidget(new QLabel(tr("Графический API:")), 0, 0);
    m_comboApi = new DarkComboBox();
    m_comboApi->addItem(tr("OpenGL (Стабильный)"), static_cast<int>(GraphicsApi::OpenGL));
    m_comboApi->addItem(tr("Vulkan (Высокий FPS + Immediate)"), static_cast<int>(GraphicsApi::Vulkan));
    g1->addWidget(m_comboApi, 0, 1);

    g1->addWidget(new QLabel(tr("Профиль рантайма:")), 1, 0);
    m_comboOptMode = new DarkComboBox();
    m_comboOptMode->addItems({"performance", "balanced", "quality"});
    g1->addWidget(m_comboOptMode, 1, 1);

    g1->addWidget(new QLabel(tr("Технология освещения:")), 2, 0);
    m_comboLighting = new DarkComboBox();
    m_comboLighting->addItem(tr("Voxel (Максимум FPS)"), static_cast<int>(LightingMode::Voxel));
    m_comboLighting->addItem(tr("ShadowMap (Мягкие тени)"), static_cast<int>(LightingMode::ShadowMap));
    m_comboLighting->addItem(tr("Future Phase 3 (Ультра-реализм)"), static_cast<int>(LightingMode::FuturePhase3));
    m_comboLighting->addItem(tr("По умолчанию плейса"), static_cast<int>(LightingMode::GameDefault));
    g1->addWidget(m_comboLighting, 2, 1);

    g1->addWidget(new QLabel(tr("Лимит кадров (FPS):")), 3, 0);
    auto *fpsRow = new QHBoxLayout();
    m_spinFps = new QSpinBox();
    m_spinFps->setRange(30, 9999);
    m_chkUnlimFps = new QCheckBox(tr("♾️ Без ограничений (9999 FPS)"));
    connect(m_chkUnlimFps, &QAbstractButton::toggled, this, [this](bool c) {
        m_spinFps->setEnabled(!c);
        triggerInstantAutosave();
    });
    fpsRow->addWidget(m_spinFps);
    fpsRow->addWidget(m_chkUnlimFps);
    g1->addLayout(fpsRow, 3, 1);

    g1->addWidget(new QLabel(tr("Качество текстур:")), 4, 0);
    m_comboTex = new DarkComboBox();
    m_comboTex->addItem(tr("Автоматически"), static_cast<int>(TextureQuality::Auto));
    m_comboTex->addItem(tr("Уровень 0 (Минимум)"), static_cast<int>(TextureQuality::Level0));
    m_comboTex->addItem(tr("Уровень 1 (Низкие)"), static_cast<int>(TextureQuality::Level1));
    m_comboTex->addItem(tr("Уровень 2 (Средние)"), static_cast<int>(TextureQuality::Level2));
    m_comboTex->addItem(tr("Уровень 3 (Ультра HD)"), static_cast<int>(TextureQuality::Level3));
    g1->addWidget(m_comboTex, 4, 1);

    g1->addWidget(new QLabel(tr("Сглаживание (MSAA):")), 5, 0);
    m_comboMsaa = new DarkComboBox();
    m_comboMsaa->addItem(tr("Отключено (0x)"), static_cast<int>(MsaaMode::Off));
    m_comboMsaa->addItem("2x MSAA", static_cast<int>(MsaaMode::X2));
    m_comboMsaa->addItem("4x MSAA", static_cast<int>(MsaaMode::X4));
    m_comboMsaa->addItem("8x MSAA (Ультра)", static_cast<int>(MsaaMode::X8));
    g1->addWidget(m_comboMsaa, 5, 1);

    g1->addWidget(new QLabel(tr("Уровень графики (FRM 0-21):")), 6, 0);
    m_spinFrm = new QSpinBox();
    m_spinFrm->setRange(0, 21);
    m_spinFrm->setSpecialValueText(tr("0 (Из настроек игры)"));
    g1->addWidget(m_spinFrm, 6, 1);
    lay->addWidget(cardApi);

    auto *cardFx = new QFrame();
    cardFx->setObjectName("GlassCard");
    auto *g2 = new QGridLayout(cardFx);
    g2->setContentsMargins(18, 16, 18, 16);
    g2->setSpacing(10);

    m_chkNoShadows    = new QCheckBox(tr("Отключить тени"));
    m_chkUltraShadows = new QCheckBox(tr("💎 Ультра-тени (LOD 5000)"));
    m_chkNoPostfx     = new QCheckBox(tr("Отключить PostFX"));
    m_chkLowMeshes    = new QCheckBox(tr("Упростить дальние модели"));
    m_chkGreySky      = new QCheckBox(tr("Упрощённое небо"));
    m_chkBringOof     = new QCheckBox(tr("Звук смерти «Oof!»"));
    m_chkRpc          = new QCheckBox(tr("Статус в Discord"));
    m_chkRegion       = new QCheckBox(tr("Показывать регион сервера"));
    m_chkGamepad      = new QCheckBox(tr("Поддержка геймпада"));
    m_chkHidpi        = new QCheckBox(tr("Масштабирование HiDPI"));

    g2->addWidget(m_chkNoShadows, 0, 0); g2->addWidget(m_chkUltraShadows, 0, 1);
    g2->addWidget(m_chkNoPostfx, 1, 0);  g2->addWidget(m_chkLowMeshes, 1, 1);
    g2->addWidget(m_chkGreySky, 2, 0);   g2->addWidget(m_chkBringOof, 2, 1);
    g2->addWidget(m_chkRpc, 3, 0);       g2->addWidget(m_chkRegion, 3, 1);
    g2->addWidget(m_chkGamepad, 4, 0);   g2->addWidget(m_chkHidpi, 4, 1);
    lay->addWidget(cardFx);

    auto *cardFf = new QFrame();
    cardFf->setObjectName("GlassCard");
    auto *ffl = new QVBoxLayout(cardFf);
    ffl->setContentsMargins(18, 16, 18, 16);
    ffl->setSpacing(10);

    auto *tff = new QLabel(tr("🔧 Пользовательские FastFlags (JSON):"));
    tff->setObjectName("SectionTitle");
    ffl->addWidget(tff);
    m_fflagsEditor = new QTextEdit();
    m_fflagsEditor->setFixedHeight(90);
    ffl->addWidget(m_fflagsEditor);
    m_lblFflagsValidation = new QLabel(tr("✓ Готов к проверке JSON"));
    m_lblFflagsValidation->setObjectName("SubText");
    ffl->addWidget(m_lblFflagsValidation);
    lay->addWidget(cardFf);

    for (QComboBox *c : {m_comboApi, m_comboOptMode, m_comboLighting, m_comboTex, m_comboMsaa}) {
        connect(c, &QComboBox::currentIndexChanged, this, [this]() { triggerInstantAutosave(); });
    }
    for (QSpinBox *s : {m_spinFps, m_spinFrm}) {
        connect(s, &QSpinBox::valueChanged, this, [this]() { triggerInstantAutosave(); });
    }
    for (QCheckBox *chk : {m_chkNoShadows, m_chkUltraShadows, m_chkNoPostfx, m_chkLowMeshes,
                           m_chkGreySky, m_chkBringOof, m_chkRpc, m_chkRegion, m_chkGamepad, m_chkHidpi}) {
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

    auto *tCam = new QLabel(tr("🔥 Игровые Твики, Видимость и Параметры Камеры (Sober FastFlags)"));
    tCam->setObjectName("SectionTitle");
    tCam->setWordWrap(true);
    cc->addWidget(tCam);

    auto *infoNote = new QLabel(tr(
        "🔄 <b>Требуется перезапуск игры:</b> все параметры ниже мгновенно записываются в <code>config.json</code>, "
        "но вступают в силу при следующем старте или нажатии кнопки <b>«🔄 Перезайти»</b>."));
    infoNote->setObjectName("SubText");
    infoNote->setWordWrap(true);
    cc->addWidget(infoNote);

    auto *zoomRow = new QHBoxLayout();
    zoomRow->setSpacing(10);
    m_chkInfZoom = new QCheckBox(tr("🔭 Базовый лимит отдаления камеры [🔄 перезапуск; работает, если плейс не блокирует Lua-скриптом]:"));
    m_spinMaxZoom = new QSpinBox();
    m_spinMaxZoom->setRange(128, 100000);
    m_spinMaxZoom->setSingleStep(500);
    m_spinMaxZoom->setFixedWidth(130);
    zoomRow->addWidget(m_chkInfZoom, 1);
    zoomRow->addWidget(m_spinMaxZoom);
    cc->addLayout(zoomRow);

    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(10);
    presetRow->addWidget(new QLabel(tr("Быстрый пресет дистанции зума:")));
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
    netRow->addWidget(new QLabel(tr("🌐 Сетевой пакетный профиль (MTU HitReg) [🔄 перезапуск]:")));
    m_comboNetOpt = new DarkComboBox();
    m_comboNetOpt->addItem(tr("⚡ Ультра-отклик без фрагментации (MTU 1396 + Fast RakNet)"), static_cast<int>(NetworkProfile::UltraLowPing));
    m_comboNetOpt->addItem(tr("Стандартный буфер (MTU 1492)"), static_cast<int>(NetworkProfile::Balanced));
    netRow->addWidget(m_comboNetOpt, 1);
    cc->addLayout(netRow);

    m_chkFullBright = new QCheckBox(tr("☀️ Высветление тёмных локаций / FullBright (Voxel + тени 0 + без PostFX) [🔄 перезапуск]"));
    m_chkClearFog   = new QCheckBox(tr("🌿 Убрать всю высокую 3D-траву на ландшафте (Чистый обзор поверхности) [🔄 перезапуск]"));
    m_chkHitboxes   = new QCheckBox(tr("📐 Упростить геометрию дальних объектов и кустов (Low-Poly LOD 0) [🔄 перезапуск]"));
    m_chkHudFps     = new QCheckBox(tr("📊 Включить счётчик FPS на экране (FFlagDebugDisplayFPS + драйверный HUD) [🔄 перезапуск]"));

    // Заблокированные на стороне Sober/Roblox флаги помечаем честно как недоступные (Disabled + пояснение)
    m_chkNoclipCam  = new QCheckBox(tr("🔒 Камера сквозь стены (PopperCam) — недоступно: флаг заблокирован фильтром безопасности Sober"));
    m_chkNoclipCam->setEnabled(false);
    m_chkWallhop    = new QCheckBox(tr("🔒 Ускорение тикрейта физики (S2PhysicsSenderRate) — недоступно: игнорируется защитой Roblox/Sober"));
    m_chkWallhop->setEnabled(false);

    for (QCheckBox *chk : {m_chkFullBright, m_chkClearFog, m_chkHitboxes, m_chkHudFps, m_chkNoclipCam, m_chkWallhop, m_chkInfZoom}) {
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

    auto *tm = new QLabel(tr("🩺 Диагностика Sober, Проверка config.json и Папка модов"));
    tm->setObjectName("SectionTitle");
    cm->addWidget(tm);

    auto *rMods = new QHBoxLayout();
    auto *bShowCfg = new AnimButton(tr("🔍 Проверить config.json"), nullptr, QColor(16, 185, 129));
    auto *bCopyDiag = new AnimButton(tr("📋 Скопировать отчёт диагностики"), nullptr, QColor(56, 189, 248));
    auto *bOpenOv  = new AnimButton(tr("📂 Папка модов"));
    auto *bClean   = new AnimButton(tr("🧹 Очистить кэш"));

    connect(bShowCfg, &QPushButton::clicked, this, [this]() {
        saveSettingsNow(true);
        QFile cf(m_settingsMgr.soberBaseDir() + "/config/sober/config.json");
        if (cf.open(QIODevice::ReadOnly)) {
            m_logView->clear();
            m_logView->append("✓ Актуальный файл ~/.var/app/org.vinegarhq.Sober/config/sober/config.json:\n");
            m_logView->append(QString::fromUtf8(cf.readAll()));
            cf.close();
        }
    });
    connect(bCopyDiag, &QPushButton::clicked, this, [this]() {
        QString cfgPath = m_settingsMgr.soberBaseDir() + "/config/sober/config.json";
        bool soberInstalled = QFileInfo::exists(m_settingsMgr.soberBaseDir());
        QString report = QString(
            "=== MqBlox v34.6 Diagnostic Report ===\n"
            "Version: v34.6 (Schema 34)\n"
            "Wayland/X11 Platform: %1\n"
            "Sober Directory Exists: %2 (%3)\n"
            "Sober config.json Exists: %4\n"
            "Backup config.json.mqblox.bak Exists: %5\n"
            "Graphics API: %6 | Target FPS: %7\n")
            .arg(QGuiApplication::platformName())
            .arg(soberInstalled ? "YES" : "NO")
            .arg(m_settingsMgr.soberBaseDir())
            .arg(QFileInfo::exists(cfgPath) ? "YES" : "NO")
            .arg(QFileInfo::exists(cfgPath + ".mqblox.bak") ? "YES" : "NO")
            .arg(m_settingsMgr.settings().graphicsApi == GraphicsApi::Vulkan ? "Vulkan" : "OpenGL")
            .arg(m_settingsMgr.settings().unlimitedFps ? 9999 : m_settingsMgr.settings().targetFps);
        QApplication::clipboard()->setText(report);
        m_logView->setPlainText(report);
        enqueueToast(tr("📋 Отчёт диагностики скопирован в буфер обмена!"));
    });
    connect(bOpenOv, &QPushButton::clicked, this, [this]() {
        QString ov = m_settingsMgr.soberBaseDir() + "/data/sober/asset_overlay";
        QDir().mkpath(ov + "/content/sounds");
        QDir().mkpath(ov + "/content/textures/Cursors/KeyboardMouse");
        QProcess::startDetached("xdg-open", {ov});
    });
    connect(bClean, &QPushButton::clicked, this, [this]() {
        QString cacheDir = m_settingsMgr.soberBaseDir() + "/cache/sober/http";
        QDir(cacheDir).removeRecursively();
        QDir().mkpath(cacheDir);
        enqueueToast(tr("🧹 Кэш карт полностью очищен!"));
    });

    rMods->addWidget(bShowCfg);
    rMods->addWidget(bCopyDiag);
    rMods->addWidget(bOpenOv);
    rMods->addWidget(bClean);
    cm->addLayout(rMods);
    lay->addWidget(cardMods);

    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->document()->setMaximumBlockCount(1000);
    m_logView->setMinimumHeight(150);
    lay->addWidget(m_logView);
    return page;
}

QWidget *MainWindow::buildUpdatesPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);
    lay->setSpacing(12);

    auto *cardInfo = new QFrame();
    cardInfo->setObjectName("GlassCard");
    auto *ci = new QVBoxLayout(cardInfo);
    ci->setContentsMargins(18, 16, 18, 16);
    ci->setSpacing(10);

    auto *title = new QLabel(tr("🔄 Центр Автообновлений MqBlox (GitHub: MrQuster/mqblox)"));
    title->setObjectName("SectionTitle");
    ci->addWidget(title);

    m_lblGitVersionInfo = new QLabel(tr("Проверка текущей версии репозитория..."));
    m_lblGitVersionInfo->setWordWrap(true);
    ci->addWidget(m_lblGitVersionInfo);

    m_updateProgressBar = new QProgressBar();
    m_updateProgressBar->setObjectName("UpdateBar");
    m_updateProgressBar->setRange(0, 100);
    m_updateProgressBar->setValue(100);
    m_updateProgressBar->setFormat(tr("Готов к проверке обновлений"));
    ci->addWidget(m_updateProgressBar);

    auto *btnRow = new QHBoxLayout();
    auto *btnCheck = new AnimButton(tr("🔍 Проверить GitHub"), nullptr, QColor(56, 189, 248));
    auto *btnPull = new AnimButton(tr("⬇️ Установить обнову с GitHub"), nullptr, QColor(16, 185, 129), true);
    auto *btnRestart = new AnimButton(tr("🚀 Перезапустить"), nullptr, QColor(245, 158, 11));

    connect(btnCheck, &QPushButton::clicked, this, [this]() { refreshGitUpdateStatus(); });
    connect(btnPull, &QPushButton::clicked, this, [this, btnPull]() {
        btnPull->setEnabled(false);
        m_updateProgressBar->setRange(0, 0);
        m_updateProgressBar->setFormat(tr("⏳ Скачивание с GitHub и компиляция..."));
        m_updateLogView->clear();
        m_updateLogView->append("==> Загрузка свежего кода с https://github.com/MrQuster/mqblox ...");

        auto *proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        proc->setWorkingDirectory(QDir::homePath() + "/mqblox_v13_src");
        connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
            m_updateLogView->append(QString::fromUtf8(proc->readAllStandardOutput()).trimmed());
        });
        connect(proc, &QProcess::finished, this, [this, proc, btnPull](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            btnPull->setEnabled(true);
            m_updateProgressBar->setRange(0, 100);
            m_updateProgressBar->setValue(100);
            if (code == 0) {
                m_updateProgressBar->setFormat(tr("✓ Обновление установлено! Перезапуск через 2 с..."));
                enqueueToast(tr("🎉 Обновление с GitHub установлено! Перезапуск..."));
                QTimer::singleShot(1800, this, [this]() {
                    m_forceQuit = true;
                    if (m_trayIcon) m_trayIcon->hide();
                    QProcess::startDetached(QDir::homePath() + "/.local/bin/mqblox-bin", {});
                    QApplication::quit();
                });
            } else {
                m_updateProgressBar->setFormat(tr("⚠️ Ошибка сборки обновления (см. журнал ниже)"));
                enqueueToast(tr("⚠️ Ошибка при обновлении"));
            }
        });
                proc->start("bash", QStringList{"-c",
            "set -e; "
            "git fetch origin main && git reset --hard origin/main; "
            "cmake -B build -S . -DCMAKE_BUILD_TYPE=Release; "
            "cmake --build build --parallel; "
            "install -Dm755 build/mqblox '$HOME/.local/bin/mqblox-bin'; "
            "ln -sf '$HOME/.local/bin/mqblox-bin' '$HOME/.local/bin/mqblox'"
        });
    });
    connect(btnRestart, &QPushButton::clicked, this, [this]() {
        m_forceQuit = true;
        if (m_trayIcon) m_trayIcon->hide();
        QProcess::startDetached(QDir::homePath() + "/.local/bin/mqblox-bin", {});
        QApplication::quit();
    });

    btnRow->addWidget(btnCheck);
    btnRow->addWidget(btnPull);
    btnRow->addWidget(btnRestart);
    ci->addLayout(btnRow);
    lay->addWidget(cardInfo);

    auto *cardLog = new QFrame();
    cardLog->setObjectName("GlassCard");
    auto *cl = new QVBoxLayout(cardLog);
    cl->setContentsMargins(18, 16, 18, 16);
    auto *tLog = new QLabel(tr("📜 Журнал обновлений GitHub:"));
    tLog->setObjectName("SectionTitle");
    cl->addWidget(tLog);

    m_updateLogView = new QTextEdit();
    m_updateLogView->setReadOnly(true);
    m_updateLogView->setMinimumHeight(200);
    cl->addWidget(m_updateLogView);
    lay->addWidget(cardLog);

    lay->addStretch();
    return page;
}

void MainWindow::refreshGitUpdateStatus() {
    if (!m_lblGitVersionInfo || !m_updateLogView) return;
    m_updateProgressBar->setRange(0, 0);
    m_updateProgressBar->setFormat(tr("⏳ Связь с GitHub (MrQuster/mqblox)..."));
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(QDir::homePath() + "/mqblox_v13_src");
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus) {
        QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        m_updateProgressBar->setRange(0, 100);
        m_updateProgressBar->setValue(100);
        if (code != 0 || out.isEmpty()) {
            m_lblGitVersionInfo->setText(tr("🟡 <b>Автономный режим — работает локальная версия MqBlox v34.6</b>"));
            m_updateProgressBar->setFormat(tr("⚠️ Нет сети или репозиторий недоступен (текущая версия сохранена)"));
            return;
        }
        bool upToDate = out.startsWith("UP_TO_DATE");
        if (upToDate) {
            m_lblGitVersionInfo->setText(tr("🟢 <b>Уже установлена последняя версия MqBlox v34.6 [HyperOS 4 Liquid Glass]</b>"));
            m_updateProgressBar->setFormat(tr("✓ Установлена актуальная версия"));
        } else {
            m_lblGitVersionInfo->setText(tr("🔵 <b>Доступно новое обновление в репозитории MrQuster/mqblox!</b>"));
            m_updateProgressBar->setFormat(tr("⬇️ Нажмите «Установить обнову с GitHub» для обновления"));
        }
        int nl = out.indexOf('\n');
        m_updateLogView->setPlainText(nl != -1 ? out.mid(nl + 1) : out);
    });
    proc->start("bash", QStringList{"-c",
        "git fetch origin main 2>/dev/null || exit 1; "
        "LOCAL=$(git rev-parse HEAD 2>/dev/null); "
        "REMOTE=$(git rev-parse origin/main 2>/dev/null); "
        "if [ \"$LOCAL\" = \"$REMOTE\" ]; then echo UP_TO_DATE; else echo UPDATE_AVAILABLE; fi; "
        "git log -n 10 --pretty=format:'✦ [%h] %s (%cr)' origin/main 2>/dev/null"
    });
}

void MainWindow::filterSettingsBySearch(const QString &query) {
    QString q = query.trimmed();
    int firstMatchedTab = -1;
    for (int i = 1; i < m_pages->count(); ++i) {
        QWidget *page = m_pages->widget(i);
        if (!page) continue;
        bool pageHasMatch = false;
        for (QFrame *card : page->findChildren<QFrame *>("GlassCard")) {
            if (q.isEmpty()) {
                card->setVisible(true);
                continue;
            }
            bool match = false;
            for (QLabel *l : card->findChildren<QLabel *>()) {
                if (l->text().contains(q, Qt::CaseInsensitive)) { match = true; break; }
            }
            if (!match) {
                for (QCheckBox *c : card->findChildren<QCheckBox *>()) {
                    if (c->text().contains(q, Qt::CaseInsensitive)) { match = true; break; }
                }
            }
            if (!match) {
                for (QComboBox *cmb : card->findChildren<QComboBox *>()) {
                    for (int k = 0; k < cmb->count(); ++k) {
                        if (cmb->itemText(k).contains(q, Qt::CaseInsensitive)) { match = true; break; }
                    }
                    if (match) break;
                }
            }
            if (!match) {
                for (QPushButton *btn : card->findChildren<QPushButton *>()) {
                    if (btn->text().contains(q, Qt::CaseInsensitive)) { match = true; break; }
                }
            }
            card->setVisible(match);
            if (match) pageHasMatch = true;
        }
        if (pageHasMatch && firstMatchedTab == -1) {
            firstMatchedTab = i;
        }
    }
    if (!q.isEmpty() && firstMatchedTab != -1 && m_pages->currentIndex() != firstMatchedTab) {
        switchTab(firstMatchedTab);
    }
}

void MainWindow::resetCurrentCategoryToDefaults() {
    AppSettings def;
    AppSettings &s = m_settingsMgr.settings();
    int idx = m_pages->currentIndex();
    if (idx == 2) { // Система и Внешний вид (все списки и чекбоксы)
        s.resMode = def.resMode; s.gpuVendor = def.gpuVendor; s.cpuVendor = def.cpuVendor;
        s.windowDriver = def.windowDriver;
        s.returnToMqBloxOnLeave = def.returnToMqBloxOnLeave; s.pauseWebInGame = def.pauseWebInGame;
        s.animBg = def.animBg; s.closeToTray = def.closeToTray; s.glThread = def.glThread;
        s.asyncShaders = def.asyncShaders; s.vsyncOff = def.vsyncOff; s.pipewireBuffer = def.pipewireBuffer;
        s.mallocTrim = def.mallocTrim; s.smartAutoCleanCache = def.smartAutoCleanCache;
        s.lowRamFflags = def.lowRamFflags; s.disableTelemetry = def.disableTelemetry;
        s.enableGamemode = def.enableGamemode; s.autoLaunchFromBrowser = def.autoLaunchFromBrowser;
        s.autoReconnectOnCrash = def.autoReconnectOnCrash; s.hideSoberDesktop = def.hideSoberDesktop;
        SettingsRegistry::resetVisualAppearance(s);
    } else if (idx == 3) { // Графика (все списки, чекбоксы и редактор FFlags)
        s.graphicsApi = def.graphicsApi; s.optMode = def.optMode; s.lighting = def.lighting;
        s.textureQuality = def.textureQuality; s.msaa = def.msaa; s.targetFps = def.targetFps;
        s.unlimitedFps = def.unlimitedFps; s.frmQualityLevel = def.frmQualityLevel;
        s.noShadows = def.noShadows; s.ultraShadows = def.ultraShadows; s.noPostFx = def.noPostFx;
        s.lowMeshes = def.lowMeshes; s.greySky = def.greySky; s.bringBackOof = def.bringBackOof;
        s.discordRpc = def.discordRpc; s.serverRegionIndicator = def.serverRegionIndicator;
        s.gamepadSupport = def.gamepadSupport; s.enableHiDpi = def.enableHiDpi;
        s.customFflagsJson = def.customFflagsJson;
    } else if (idx == 4) { // Игровые твики
        s.camZoom = def.camZoom; s.unlockInfiniteZoom = def.unlockInfiniteZoom;
        s.customMaxZoom = def.customMaxZoom; s.fullBright = def.fullBright;
        s.clearFog = def.clearFog; s.showHitboxes = def.showHitboxes;
        s.showHudFps = def.showHudFps; s.netProfile = def.netProfile;
    }
    syncUiFromSettings();
    saveSettingsNow(true);
    enqueueToast(tr("↺ Все параметры текущей вкладки сброшены по умолчанию!"));
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

    // Динамический QSS для QLabel, QSpinBox, QLineEdit, QTextEdit и QSlider (исправляет читаемость в Светлой теме и стилизует ползунки)
    QString dynQss = QString(
        "QLabel { color: %1; font-size: %2px; font-weight: %3; font-style: normal; background: transparent; }\n"
        "QLabel#SubText { color: %4; font-size: %5px; }\n"
        "QSpinBox, QLineEdit { color: %1; font-size: %2px; font-weight: 700; background: transparent; border: none; padding: 6px 12px; }\n"
        "QTextEdit { color: %1; background-color: %6; border: 1px solid rgba(%7, %8, %9, 95); border-radius: 14px; padding: 10px; }\n"
        "QSlider::groove:horizontal { height: 6px; background: rgba(148, 163, 184, 55); border-radius: 3px; }\n"
        "QSlider::sub-page:horizontal { background: rgba(%7, %8, %9, 195); border-radius: 3px; }\n"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; "
        "background: #FFFFFF; border: 2px solid rgb(%7, %8, %9); }\n"
    ).arg(mainTxtHex)
     .arg(scaledPx)
     .arg(fontWeight)
     .arg(subTxtHex)
     .arg(std::max(10, scaledPx - 2))
     .arg(lightTheme ? "rgba(255, 255, 255, 140)" : "rgba(6, 10, 20, 45)")
     .arg(acc.red()).arg(acc.green()).arg(acc.blue());

    if (m_pages) m_pages->setStyleSheet(dynQss);

    if (m_sidebar) {
        bool compact = !s.sidebarShowLabels || (s.sidebarPosition == 1) || (!s.sidebarPinned && !m_sidebar->underMouse());
        int targetW = compact ? 68 : std::clamp(s.sidebarWidthPx, 160, 280);
        m_sidebar->setMinimumWidth(targetW);
        m_sidebar->setMaximumWidth(targetW);
        for (auto *b : m_navButtons) {
            b->setCompactMode(compact);
        }
    }

    int padV = (s.uiDensity == 0) ? 6 : ((s.uiDensity == 2) ? 16 : 12);
    for (int i = 1; i < m_pages->count(); ++i) {
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

    m_comboRes->setCurrentIndex(m_comboRes->findData(static_cast<int>(s.resMode)));
    m_comboGpuVendor->setCurrentIndex(m_comboGpuVendor->findData(static_cast<int>(s.gpuVendor)));
    m_comboCpuVendor->setCurrentIndex(m_comboCpuVendor->findData(static_cast<int>(s.cpuVendor)));
    m_comboWinDrv->setCurrentIndex(m_comboWinDrv->findData(static_cast<int>(s.windowDriver)));
    m_comboApi->setCurrentIndex(m_comboApi->findData(static_cast<int>(s.graphicsApi)));
    m_comboOptMode->setCurrentText(s.optMode);
    m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(s.lighting)));
    m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(s.textureQuality)));
    m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(s.msaa)));
    m_comboCamZoom->setCurrentIndex(m_comboCamZoom->findData(static_cast<int>(s.camZoom)));
    m_comboNetOpt->setCurrentIndex(m_comboNetOpt->findData(static_cast<int>(s.netProfile)));

    m_spinFps->setValue(s.targetFps);
    m_chkUnlimFps->setChecked(s.unlimitedFps);
    m_spinFps->setEnabled(!s.unlimitedFps);
    m_spinFrm->setValue(s.frmQualityLevel);

    m_chkReturnMq->setChecked(s.returnToMqBloxOnLeave);
    m_chkPauseWeb->setChecked(s.pauseWebInGame);
    m_chkAnimBg->setChecked(s.animBg);
    m_chkCloseToTray->setChecked(s.closeToTray);
    m_chkGlThread->setChecked(s.glThread);
    m_chkAsyncShaders->setChecked(s.asyncShaders);
    m_chkVsyncOff->setChecked(s.vsyncOff);
    m_chkPwBuffer->setChecked(s.pipewireBuffer);
    m_chkMallocTrim->setChecked(s.mallocTrim);
    m_chkAutoClean->setChecked(s.smartAutoCleanCache);
    m_chkMemFflags->setChecked(s.lowRamFflags);
    m_chkNoTelemetry->setChecked(s.disableTelemetry);
    m_chkGamemode->setChecked(s.enableGamemode);
    m_chkAutoLaunch->setChecked(s.autoLaunchFromBrowser);
    m_chkAutoReconnect->setChecked(s.autoReconnectOnCrash);
    m_chkHideSober->setChecked(s.hideSoberDesktop);

    m_chkNoShadows->setChecked(s.noShadows);
    m_chkUltraShadows->setChecked(s.ultraShadows);
    m_chkNoPostfx->setChecked(s.noPostFx);
    m_chkLowMeshes->setChecked(s.lowMeshes);
    m_chkGreySky->setChecked(s.greySky);
    m_chkBringOof->setChecked(s.bringBackOof);
    m_chkRpc->setChecked(s.discordRpc);
    m_chkRegion->setChecked(s.serverRegionIndicator);
    m_chkGamepad->setChecked(s.gamepadSupport);
    m_chkHidpi->setChecked(s.enableHiDpi);

    if (m_chkHudFps) m_chkHudFps->setChecked(s.showHudFps);
    if (m_chkClearFog) m_chkClearFog->setChecked(s.clearFog);
    if (m_spinMaxZoom) m_spinMaxZoom->setValue(s.customMaxZoom);
    if (m_chkInfZoom) m_chkInfZoom->setChecked(s.unlockInfiniteZoom);
    if (m_chkNoclipCam) m_chkNoclipCam->setChecked(false);
    if (m_chkFullBright) m_chkFullBright->setChecked(s.fullBright);
    if (m_chkHitboxes) m_chkHitboxes->setChecked(s.showHitboxes);
    if (m_chkWallhop) m_chkWallhop->setChecked(false);

    // Stage 3 Visual Controls
    if (m_comboVisualPreset) m_comboVisualPreset->setCurrentIndex(m_comboVisualPreset->findData(s.visualPreset));
    if (m_sldBlur) m_sldBlur->setValueSilent(s.glassBlurStrength);
    if (m_sldOpacity) m_sldOpacity->setValueSilent(s.glassOpacityPct);
    if (m_sldBorderThick) m_sldBorderThick->setValueSilent(s.borderThicknessPx);
    if (m_sldBorderBright) m_sldBorderBright->setValueSilent(s.borderBrightnessPct);
    if (m_sldSaturation) m_sldSaturation->setValueSilent(s.glassSaturationPct);
    if (m_sldSpecular) m_sldSpecular->setValueSilent(s.specularStrengthPct);
    if (m_sldShadow) m_sldShadow->setValueSilent(s.shadowStrengthPct);
    if (m_comboCornerRadius) m_comboCornerRadius->setCurrentIndex(m_comboCornerRadius->findData(s.cornerRadiusMode));
    if (m_comboGlassStyle) m_comboGlassStyle->setCurrentIndex(m_comboGlassStyle->findData(s.glassStyleMode));

    if (m_comboThemeMode) m_comboThemeMode->setCurrentIndex(m_comboThemeMode->findData(s.themeMode));
    if (m_comboAccent) m_comboAccent->setCurrentIndex(m_comboAccent->findData(s.accentPreset));
    if (m_inpCustomHex) m_inpCustomHex->setText(s.customAccentHex);
    if (m_chkChameleon) m_chkChameleon->setChecked(s.chameleonEnabled);
    if (m_sldChamSpeed) m_sldChamSpeed->setValueSilent(s.chameleonSpeedPct);
    if (m_sldChamStrength) m_sldChamStrength->setValueSilent(s.chameleonStrengthPct);
    if (m_comboBorderSource) m_comboBorderSource->setCurrentIndex(m_comboBorderSource->findData(s.borderColorSource));
    if (m_comboTextColor) m_comboTextColor->setCurrentIndex(m_comboTextColor->findData(s.textColorMode));

    if (m_comboBackdropType) m_comboBackdropType->setCurrentIndex(m_comboBackdropType->findData(s.backdropType));
    if (m_sldBackdropDim) m_sldBackdropDim->setValueSilent(s.backdropDimPct);
    if (m_comboVideoFps) {
        int fIdx = m_comboVideoFps->findData(s.videoFpsLimit);
        m_comboVideoFps->setCurrentIndex(fIdx >= 0 ? fIdx : 0);
    }
    if (m_chkPauseFocusLoss) m_chkPauseFocusLoss->setChecked(s.pauseOnFocusLoss);

    if (m_chkAnimEnabled) m_chkAnimEnabled->setChecked(s.animationsEnabled);
    if (m_sldAnimSpeed) m_sldAnimSpeed->setValueSilent(s.animSpeedPct);
    if (m_comboAnimStyle) m_comboAnimStyle->setCurrentIndex(m_comboAnimStyle->findData(s.animStyle));
    if (m_chkCursorGlow) m_chkCursorGlow->setChecked(s.cursorGlowFollow);
    if (m_chkReducedMotion) m_chkReducedMotion->setChecked(s.reducedMotion);

    if (m_sldUiScale) m_sldUiScale->setValueSilent(s.uiScalePct);
    if (m_sldFontSize) m_sldFontSize->setValueSilent(s.fontSizePx);
    if (m_comboFontBold) m_comboFontBold->setCurrentIndex(m_comboFontBold->findData(s.fontBoldness));
    if (m_comboUiDensity) m_comboUiDensity->setCurrentIndex(m_comboUiDensity->findData(s.uiDensity));

    if (m_comboSidebarPos) m_comboSidebarPos->setCurrentIndex(m_comboSidebarPos->findData(s.sidebarPosition));
    if (m_sldSidebarWidth) m_sldSidebarWidth->setValueSilent(s.sidebarWidthPx);
    if (m_chkSidebarLabels) m_chkSidebarLabels->setChecked(s.sidebarShowLabels);
    if (m_chkRememberWinSize) m_chkRememberWinSize->setChecked(s.rememberWindowSize);
    if (m_chkStartMinimized) m_chkStartMinimized->setChecked(s.startMinimized);
    if (m_chkAlwaysOnTop && m_chkAlwaysOnTop->isEnabled()) m_chkAlwaysOnTop->setChecked(s.alwaysOnTop);

    if (m_comboEffectsQuality) m_comboEffectsQuality->setCurrentIndex(m_comboEffectsQuality->findData(s.effectsQuality));
    if (m_chkHighContrast) m_chkHighContrast->setChecked(s.highContrast);
    if (m_chkReduceTransp) m_chkReduceTransp->setChecked(s.reduceTransparency);
    if (m_chkDisableBlurGpu) m_chkDisableBlurGpu->setChecked(s.disableBlurLowGpu);

    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    m_inpPlace->setText(s.lastPlaceInput);
    m_fflagsEditor->setPlainText(s.customFflagsJson);
    m_btnPinSide->setText(s.sidebarPinned ? "📌" : "📍");
    m_isLoadingUi = false;
}

void MainWindow::collectSettingsFromUi() {
    AppSettings &s = m_settingsMgr.settings();
    s.resMode = static_cast<ResolutionMode>(m_comboRes->currentData().toInt());
    s.gpuVendor = static_cast<GpuVendorMode>(m_comboGpuVendor->currentData().toInt());
    s.cpuVendor = static_cast<CpuVendorMode>(m_comboCpuVendor->currentData().toInt());
    s.windowDriver = static_cast<WindowDriverMode>(m_comboWinDrv->currentData().toInt());
    s.graphicsApi = static_cast<GraphicsApi>(m_comboApi->currentData().toInt());
    s.optMode = m_comboOptMode->currentText();
    s.lighting = static_cast<LightingMode>(m_comboLighting->currentData().toInt());
    s.textureQuality = static_cast<TextureQuality>(m_comboTex->currentData().toInt());
    s.msaa = static_cast<MsaaMode>(m_comboMsaa->currentData().toInt());
    s.camZoom = static_cast<CameraZoomMode>(m_comboCamZoom->currentData().toInt());
    s.netProfile = static_cast<NetworkProfile>(m_comboNetOpt->currentData().toInt());

    s.targetFps = m_spinFps->value();
    s.unlimitedFps = m_chkUnlimFps->isChecked();
    s.frmQualityLevel = m_spinFrm->value();

    s.audioSink = "default";
    s.audioSource = "default";

    s.returnToMqBloxOnLeave = m_chkReturnMq->isChecked();
    s.pauseWebInGame = m_chkPauseWeb->isChecked();
    s.animBg = m_chkAnimBg->isChecked();
    s.closeToTray = m_chkCloseToTray->isChecked();
    s.glThread = m_chkGlThread->isChecked();
    s.asyncShaders = m_chkAsyncShaders->isChecked();
    s.vsyncOff = m_chkVsyncOff->isChecked();
    s.pipewireBuffer = m_chkPwBuffer->isChecked();
    s.mallocTrim = m_chkMallocTrim->isChecked();
    s.smartAutoCleanCache = m_chkAutoClean->isChecked();
    s.lowRamFflags = m_chkMemFflags->isChecked();
    s.disableTelemetry = m_chkNoTelemetry->isChecked();
    s.enableGamemode = m_chkGamemode->isChecked();
    s.autoLaunchFromBrowser = m_chkAutoLaunch->isChecked();
    s.autoReconnectOnCrash = m_chkAutoReconnect->isChecked();
    s.hideSoberDesktop = m_chkHideSober->isChecked();

    s.noShadows = m_chkNoShadows->isChecked();
    s.ultraShadows = m_chkUltraShadows->isChecked();
    s.noPostFx = m_chkNoPostfx->isChecked();
    s.lowMeshes = m_chkLowMeshes->isChecked();
    s.greySky = m_chkGreySky->isChecked();
    s.bringBackOof = m_chkBringOof->isChecked();
    s.discordRpc = m_chkRpc->isChecked();
    s.serverRegionIndicator = m_chkRegion->isChecked();
    s.gamepadSupport = m_chkGamepad->isChecked();
    s.enableHiDpi = m_chkHidpi->isChecked();

    if (m_chkHudFps) s.showHudFps = m_chkHudFps->isChecked();
    if (m_chkClearFog) s.clearFog = m_chkClearFog->isChecked();
    if (m_spinMaxZoom) s.customMaxZoom = m_spinMaxZoom->value();
    if (m_chkInfZoom) s.unlockInfiniteZoom = m_chkInfZoom->isChecked();
    if (m_chkFullBright) s.fullBright = m_chkFullBright->isChecked();
    if (m_chkHitboxes) s.showHitboxes = m_chkHitboxes->isChecked();

    if (m_comboVisualPreset) s.visualPreset = m_comboVisualPreset->currentData().toInt();
    if (m_sldBlur) s.glassBlurStrength = m_sldBlur->value();
    if (m_sldOpacity) s.glassOpacityPct = m_sldOpacity->value();
    if (m_sldBorderThick) s.borderThicknessPx = m_sldBorderThick->value();
    if (m_sldBorderBright) s.borderBrightnessPct = m_sldBorderBright->value();
    if (m_sldSaturation) s.glassSaturationPct = m_sldSaturation->value();
    if (m_sldSpecular) s.specularStrengthPct = m_sldSpecular->value();
    if (m_sldShadow) s.shadowStrengthPct = m_sldShadow->value();
    if (m_comboCornerRadius) s.cornerRadiusMode = m_comboCornerRadius->currentData().toInt();
    if (m_comboGlassStyle) s.glassStyleMode = m_comboGlassStyle->currentData().toInt();

    if (m_comboThemeMode) s.themeMode = m_comboThemeMode->currentData().toInt();
    if (m_comboAccent) s.accentPreset = m_comboAccent->currentData().toInt();
    if (m_inpCustomHex) s.customAccentHex = m_inpCustomHex->text().trimmed();
    if (m_chkChameleon) s.chameleonEnabled = m_chkChameleon->isChecked();
    if (m_sldChamSpeed) s.chameleonSpeedPct = m_sldChamSpeed->value();
    if (m_sldChamStrength) s.chameleonStrengthPct = m_sldChamStrength->value();
    if (m_comboBorderSource) s.borderColorSource = m_comboBorderSource->currentData().toInt();
    if (m_comboTextColor) s.textColorMode = m_comboTextColor->currentData().toInt();

    if (m_comboBackdropType) s.backdropType = m_comboBackdropType->currentData().toInt();
    if (m_sldBackdropDim) s.backdropDimPct = m_sldBackdropDim->value();
    if (m_comboVideoFps) s.videoFpsLimit = m_comboVideoFps->currentData().toInt();
    if (m_chkPauseFocusLoss) s.pauseOnFocusLoss = m_chkPauseFocusLoss->isChecked();

    if (m_chkAnimEnabled) s.animationsEnabled = m_chkAnimEnabled->isChecked();
    if (m_sldAnimSpeed) s.animSpeedPct = m_sldAnimSpeed->value();
    if (m_comboAnimStyle) s.animStyle = m_comboAnimStyle->currentData().toInt();
    if (m_chkCursorGlow) s.cursorGlowFollow = m_chkCursorGlow->isChecked();
    if (m_chkReducedMotion) s.reducedMotion = m_chkReducedMotion->isChecked();

    if (m_sldUiScale) s.uiScalePct = m_sldUiScale->value();
    if (m_sldFontSize) s.fontSizePx = m_sldFontSize->value();
    if (m_comboFontBold) s.fontBoldness = m_comboFontBold->currentData().toInt();
    if (m_comboUiDensity) s.uiDensity = m_comboUiDensity->currentData().toInt();

    if (m_comboSidebarPos) s.sidebarPosition = m_comboSidebarPos->currentData().toInt();
    if (m_sldSidebarWidth) s.sidebarWidthPx = m_sldSidebarWidth->value();
    if (m_chkSidebarLabels) s.sidebarShowLabels = m_chkSidebarLabels->isChecked();
    if (m_chkRememberWinSize) s.rememberWindowSize = m_chkRememberWinSize->isChecked();
    if (m_chkStartMinimized) s.startMinimized = m_chkStartMinimized->isChecked();
    if (m_chkAlwaysOnTop && m_chkAlwaysOnTop->isEnabled()) s.alwaysOnTop = m_chkAlwaysOnTop->isChecked();

    if (m_comboEffectsQuality) s.effectsQuality = m_comboEffectsQuality->currentData().toInt();
    if (m_chkHighContrast) s.highContrast = m_chkHighContrast->isChecked();
    if (m_chkReduceTransp) s.reduceTransparency = m_chkReduceTransp->isChecked();
    if (m_chkDisableBlurGpu) s.disableBlurLowGpu = m_chkDisableBlurGpu->isChecked();

    if (s.rememberWindowSize && width() >= 820 && height() >= 540) {
        s.savedWindowWidth = width();
        s.savedWindowHeight = height();
    }

    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    s.lastPlaceInput = m_inpPlace->text().trimmed();
    s.customFflagsJson = m_fflagsEditor->toPlainText();
}

void MainWindow::saveSettingsNow(bool manualClick) {
    collectSettingsFromUi();
    QString err;
    if (m_settingsMgr.saveAll(&err)) {
        m_lblFflagsValidation->setText(tr("✓ Синтаксис FastFlags корректен и сохранён"));
        m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
        if (m_btnSaveSettings) {
            m_btnSaveSettings->setText(tr("✓ Настройки сохранены"));
            m_btnSaveSettings->setAccentColor(QColor(16, 185, 129), manualClick);
        }
        if (m_lblAutosync) {
            m_lblAutosync->setText(tr("✓ Все изменения сохранены"));
        }
        if (manualClick) {
            enqueueToast(tr("💾 Все настройки успешно сохранены и применены!"));
        }
    } else {
        m_lblFflagsValidation->setText("⚠️ " + err);
        m_lblFflagsValidation->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 700;");
        enqueueToast("⚠️ " + err);
    }
}

void MainWindow::triggerInstantAutosave(const QString &toastMsg) {
    if (m_isLoadingUi) return;
    if (!toastMsg.isEmpty()) enqueueToast(toastMsg);
    // МГНОВЕННО считываем все галочки и записываем в settings_v13.json и config.json!
    collectSettingsFromUi();
    QString err;
    if (m_settingsMgr.saveAll(&err)) {
        if (m_lblFflagsValidation) {
            m_lblFflagsValidation->setText(tr("✓ Настройки и чит-флаги мгновенно сохранены в config.json"));
            m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
        }
        if (m_btnSaveSettings) {
            m_btnSaveSettings->setText(tr("💾 Сохранить настройки ●"));
            m_btnSaveSettings->setAccentColor(QColor(245, 158, 11), true);
        }
        if (m_lblAutosync) {
            m_lblAutosync->setText(tr("⚡ Авто-сохранено"));
        }
    }
    m_autosaveTimer->start(600);
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

void MainWindow::setupWebProfile(const QString &accId) {
    if (m_webView) m_webView->setPage(nullptr);
    if (m_webPage) { m_webPage->deleteLater(); m_webPage = nullptr; }
    if (m_webProfile) { m_webProfile->deleteLater(); m_webProfile = nullptr; }

    QString profDir = m_settingsMgr.mqDir() + "/web_profiles_v13/" + accId;
    QDir().mkpath(profDir);
    m_webProfile = new QWebEngineProfile(QString("mqblox_v13_%1").arg(accId), this);
    m_webProfile->setPersistentStoragePath(profDir);
    m_webProfile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_webProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_webProfile->setHttpCacheMaximumSize(256 * 1024 * 1024);
    m_webProfile->setHttpUserAgent("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36");

    connect(m_webProfile->cookieStore(), &QWebEngineCookieStore::cookieAdded, this,
            [this](const QNetworkCookie &cookie) {
                if (m_syncCookieGuard) return;
                if (cookie.name() == ".ROBLOSECURITY" && cookie.domain().endsWith("roblox.com")) {
                    QString val = QString::fromUtf8(cookie.value()).trimmed();
                    if (val.size() > 30) {
                        QString curId = m_settingsMgr.activeAccountId();
                        if (SecretStore::loadToken(curId) != val) {
                            m_syncCookieGuard = true;
                            SecretStore::storeToken(curId, val);
                            syncActiveProfile(false);
                            m_syncCookieGuard = false;
                        }
                    }
                }
            });

    m_webPage = new MqWebPage(m_webProfile, this);
    m_webPage->setBackgroundColor(QColor(3, 4, 8));
    m_webPage->onRobloxUri = [this](const QString &uri) {
        saveSettingsNow(false);
        m_soberCtrl->launchRoblox(uri);
    };
    m_webView->setPage(m_webPage);
    m_webView->setUrl(QUrl("https://www.roblox.com/home"));
}

void MainWindow::onWebPageLoaded(bool ok) {
    if (!ok || !m_webView || !m_webView->page()) return;
    QString js = R"(
        (function() {
            const m = document.querySelector('meta[name="user-data"]');
            const r = document.querySelector('#nav-robux-amount');
            return JSON.stringify({
                uid: m ? m.getAttribute('data-userid') : '',
                name: m ? m.getAttribute('data-name') : '',
                disp: m ? m.getAttribute('data-displayname') : '',
                robux: r ? r.innerText : ''
            });
        })();
    )";
    m_webView->page()->runJavaScript(js, [this](const QVariant &v) {
        auto obj = QJsonDocument::fromJson(v.toString().toUtf8()).object();
        qulonglong uid = obj["uid"].toString().toULongLong();
        if (uid == 0) return;
        if (auto *acc = m_settingsMgr.activeAccount()) {
            bool changed = (acc->userId != uid);
            acc->userId = uid;
            if (!obj["name"].toString().isEmpty()) acc->username = obj["name"].toString();
            if (!obj["disp"].toString().isEmpty()) acc->displayName = obj["disp"].toString();
            if (!obj["robux"].toString().isEmpty()) {
                acc->robux = obj["robux"].toString().trimmed();
                m_lblRobux->setText("◈ R$ " + acc->robux);
            }
            m_settingsMgr.saveAll();
            if (changed) syncActiveProfile(false);
        }
    });
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
        QString badge = isAct ? "🟢 [АКТИВЕН]" : "⚪ [В РЕЗЕРВЕ]";
        auto *lbl = new QLabel(QString("%1  <b>%2</b> %3  •  Баланс: <span style='color:#34D399;'>R$ %4</span>")
                                   .arg(badge, acc.displayName, acc.username.isEmpty() ? "" : "(@" + acc.username + ")", acc.robux));
        row->addWidget(lbl);
        row->addStretch();
        if (!isAct) {
            auto *bSw = new AnimButton(tr("✅ Переключить"));
            QString id = acc.id;
            connect(bSw, &QPushButton::clicked, this, [this, id]() { m_soberCtrl->switchAccountSlotAsync(id); });
            row->addWidget(bSw);
        }
        m_accountsListLayout->addWidget(card);
    }
}

void MainWindow::syncActiveProfile(bool userInitiated) {
    auto *acc = m_settingsMgr.activeAccount();
    if (!acc) return;
    m_lblSkinDisp->setText("👤 " + acc->displayName);
    m_lblRobux->setText("◈ R$ " + acc->robux);
    QString token = SecretStore::loadToken(acc->id);
    if (userInitiated) enqueueToast(tr("🔄 Синхронизация профиля и баланса Robux..."));
    m_apiClient->fetchUserProfileAnd3D(acc->userId, token);
}

void MainWindow::applyResolutionMode(ResolutionMode mode, bool initial) {
    QScreen *scr = windowHandle() && windowHandle()->screen() ? windowHandle()->screen() : QApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1366, 768);
    int sw = avail.width(), sh = avail.height();
    setMaximumSize(sw, sh);

    int tw = 1100, th = 660;
    double zoom = 0.95;

    switch (mode) {
    case ResolutionMode::HD_720p:  tw = 1080; th = 640; zoom = 0.90; break;
    case ResolutionMode::FHD_1080p: tw = 1360; th = 780; zoom = 1.00; break;
    case ResolutionMode::QHD_2K:   tw = 1720; th = 960; zoom = 1.15; m_chkHidpi->setChecked(true); break;
    case ResolutionMode::Auto:
        tw = int(sw * 0.78);
        th = int(sh * 0.78);
        zoom = (sw >= 2200) ? 1.15 : ((sw >= 1600) ? 1.00 : 0.88);
        break;
    }

    int fw = std::clamp(tw, 840, std::max(840, sw - 32));
    int fh = std::clamp(th, 520, std::max(520, sh - 36));

    if (!isMaximized()) {
        resize(fw, fh);
        move(avail.x() + (sw - fw) / 2, avail.y() + (sh - fh) / 2);
    }
    if (m_webView) m_webView->setZoomFactor(zoom);
    if (!initial) triggerInstantAutosave(tr("🖥️ Размер окна адаптирован под экран (%1×%2)").arg(fw).arg(fh));
}

void MainWindow::applyGraphicsMasterPreset(int presetIdx) {
    m_isLoadingUi = true;
    if (presetIdx == 0) {
        m_comboOptMode->setCurrentText("performance");
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::Voxel)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level0)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::Off)));
        m_spinFrm->setValue(1);
        m_chkNoShadows->setChecked(true); m_chkUltraShadows->setChecked(false);
        m_chkNoPostfx->setChecked(true);  m_chkLowMeshes->setChecked(true);
        m_chkGreySky->setChecked(true);   m_chkUnlimFps->setChecked(true);
    } else if (presetIdx == 1) {
        m_comboOptMode->setCurrentText("balanced");
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::ShadowMap)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level2)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::X2)));
        m_spinFrm->setValue(10);
        m_chkNoShadows->setChecked(false); m_chkUltraShadows->setChecked(false);
        m_chkNoPostfx->setChecked(false);  m_chkLowMeshes->setChecked(false);
        m_chkGreySky->setChecked(false);
    } else {
        m_comboOptMode->setCurrentText("quality");
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::FuturePhase3)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level3)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::X8)));
        m_spinFrm->setValue(21);
        m_chkNoShadows->setChecked(false); m_chkUltraShadows->setChecked(true);
        m_chkNoPostfx->setChecked(false);  m_chkLowMeshes->setChecked(false);
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
