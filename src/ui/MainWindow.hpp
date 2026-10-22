#pragma once
#include "core/SecretStore.hpp"
#include "core/SettingsManager.hpp"
#include "core/SoberController.hpp"
#include "net/RobloxApiClient.hpp"
#include "render/Ton618GlWidget.hpp"
#include "ui/Widgets.hpp"
#include <QCheckBox>
#include <QCloseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QQueue>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <vector>

namespace mqblox {

class MqWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    std::function<void(const QString &)> onRobloxUri;
    explicit MqWebPage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent) {}

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override {
        const QString scheme = url.scheme().toLower();
        if (scheme == "roblox" || scheme == "roblox-player") {
            if (onRobloxUri) onRobloxUri(url.toString());
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &launchUri = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    Qt::Edges detectResizeEdges(const QPointF &pos) const;
    void initUi();
    void initTrayAndShortcuts();
    QWidget *buildHomeRobloxPage();
    QWidget *buildAccountsPage();
    QWidget *buildHardwareAudioPage();
    QWidget *buildGraphicsPage();
    QWidget *buildCameraModsPage();
    QWidget *buildUpdatesPage();

    void syncUiFromSettings();
    void collectSettingsFromUi();
    void triggerInstantAutosave(const QString &toastMsg = {});
    void saveSettingsNow(bool manualClick = true);
    void enqueueToast(const QString &msg);
    void processNextToast();
    void setupWebProfile(const QString &accId);
    void onWebPageLoaded(bool ok);
    void refreshAccountsUi();
    void syncActiveProfile(bool userInitiated);
    void applyResolutionMode(ResolutionMode mode, bool initial);
    void applyGraphicsMasterPreset(int presetIdx);
    void toggleSidebarPinMode();
    void animateSidebar(bool expand);
    void switchTab(int idx);
    void runWatchdogTick();
    void refreshGitUpdateStatus();
    void runMicrophoneDiagnostics();
    struct SearchEntry {
        int tabIndex = 0;
        QFrame *card = nullptr;
        QString haystackLower;
    };
    std::vector<SearchEntry> m_searchIndex;
    QTimer *m_searchDebounce = nullptr;
    QString m_lastAppliedQss;
    void buildSearchIndex();

    QString m_launchUri;
    SettingsManager m_settingsMgr;
    SoberController *m_soberCtrl = nullptr;
    RobloxApiClient *m_apiClient = nullptr;

    Ton618GlWidget *m_rootCosmos = nullptr;
    HoverSidebar *m_sidebar = nullptr;
    AnimButton *m_btnPinSide = nullptr;
    QWidget *m_titleColWidget = nullptr;
    QLabel *m_pinHintLbl = nullptr;
    QFrame *m_authorsBox = nullptr;
    QProgressBar *m_ramBar = nullptr;
    QLabel *m_statusBadge = nullptr;
    std::vector<AnimNavButton *> m_navButtons;

    QLabel *m_toastLbl = nullptr;
    DarkComboBox *m_comboRes = nullptr;
    DarkComboBox *m_comboQuickAcc = nullptr;
    AnimButton *m_btnMax = nullptr;
    AnimButton *m_btnSaveSettings = nullptr;
    QLabel *m_lblAutosync = nullptr;
    QStackedWidget *m_pages = nullptr;

    QLineEdit *m_inpPlace = nullptr;
    QLabel *m_lblSkinDisp = nullptr;
    QLabel *m_lblSkinUser = nullptr;
    QLabel *m_lblRobux = nullptr;
    QWebEngineView *m_webView = nullptr;
    QWebEngineProfile *m_webProfile = nullptr;
    MqWebPage *m_webPage = nullptr;

    QVBoxLayout *m_accountsListLayout = nullptr;
    QLabel *m_hwBannerLbl = nullptr;
    DarkComboBox *m_comboGpuVendor = nullptr;
    DarkComboBox *m_comboCpuVendor = nullptr;
    DarkComboBox *m_comboAudioOut = nullptr;
    DarkComboBox *m_comboAudioIn = nullptr;
    DarkComboBox *m_comboWinDrv = nullptr;

    QCheckBox *m_chkReturnMq = nullptr;
    QCheckBox *m_chkPauseWeb = nullptr;
    QCheckBox *m_chkAnimBg = nullptr;
    QCheckBox *m_chkCloseToTray = nullptr;
    QCheckBox *m_chkGlThread = nullptr;
    QCheckBox *m_chkAsyncShaders = nullptr;
    QCheckBox *m_chkVsyncOff = nullptr;
    QCheckBox *m_chkPwBuffer = nullptr;
    QCheckBox *m_chkMallocTrim = nullptr;
    QCheckBox *m_chkAutoClean = nullptr;
    QCheckBox *m_chkMemFflags = nullptr;
    QCheckBox *m_chkNoTelemetry = nullptr;
    QCheckBox *m_chkGamemode = nullptr;
    QCheckBox *m_chkAutoLaunch = nullptr;
    QCheckBox *m_chkAutoReconnect = nullptr;
    QCheckBox *m_chkHideSober = nullptr;

    QLabel *m_lblPresetDesc = nullptr;
    DarkComboBox *m_comboApi = nullptr;
    DarkComboBox *m_comboOptMode = nullptr;
    DarkComboBox *m_comboLighting = nullptr;
    QSpinBox *m_spinFps = nullptr;
    QCheckBox *m_chkUnlimFps = nullptr;
    DarkComboBox *m_comboTex = nullptr;
    DarkComboBox *m_comboMsaa = nullptr;
    QSpinBox *m_spinFrm = nullptr;

    QCheckBox *m_chkNoShadows = nullptr;
    QCheckBox *m_chkUltraShadows = nullptr;
    QCheckBox *m_chkNoPostfx = nullptr;
    QCheckBox *m_chkLowMeshes = nullptr;
    QCheckBox *m_chkGreySky = nullptr;
    QCheckBox *m_chkBringOof = nullptr;
    QCheckBox *m_chkRpc = nullptr;
    QCheckBox *m_chkRegion = nullptr;
    QCheckBox *m_chkGamepad = nullptr;
    QCheckBox *m_chkHidpi = nullptr;

    QTextEdit *m_fflagsEditor = nullptr;
    QLabel *m_lblFflagsValidation = nullptr;

    DarkComboBox *m_comboCamZoom = nullptr;
    DarkComboBox *m_comboNetOpt = nullptr;
    QCheckBox *m_chkHudFps = nullptr;
    QCheckBox *m_chkClearFog = nullptr;
    QSpinBox *m_spinMaxZoom = nullptr;
    QCheckBox *m_chkInfZoom = nullptr;
    QCheckBox *m_chkNoclipCam = nullptr;
    QCheckBox *m_chkFullBright = nullptr;
    QCheckBox *m_chkHitboxes = nullptr;
    QCheckBox *m_chkWallhop = nullptr;
    QLineEdit *m_inpSearchSettings = nullptr;
    DarkComboBox *m_comboBlurStrength = nullptr;
    QSpinBox *m_spinGlassOpacity = nullptr;
    QSpinBox *m_spinBorderBright = nullptr;
    QCheckBox *m_chkChameleon = nullptr;
    QCheckBox *m_chkReducedMotion = nullptr;
    DarkComboBox *m_comboVideoFps = nullptr;
    DarkComboBox *m_comboAccent = nullptr;
    void filterSettingsBySearch(const QString &query);
    void resetCurrentCategoryToDefaults();
    // Stage 3 Visual & Layout Controls (v34.5)
    DarkComboBox *m_comboVisualPreset = nullptr;
    ResettableSlider *m_sldBlur = nullptr;
    ResettableSlider *m_sldOpacity = nullptr;
    ResettableSlider *m_sldBorderThick = nullptr;
    ResettableSlider *m_sldBorderBright = nullptr;
    ResettableSlider *m_sldSaturation = nullptr;
    ResettableSlider *m_sldSpecular = nullptr;
    ResettableSlider *m_sldShadow = nullptr;
    DarkComboBox *m_comboCornerRadius = nullptr;
    DarkComboBox *m_comboGlassStyle = nullptr;

    DarkComboBox *m_comboThemeMode = nullptr;
    QLineEdit *m_inpCustomHex = nullptr;
    ResettableSlider *m_sldChamSpeed = nullptr;
    ResettableSlider *m_sldChamStrength = nullptr;
    DarkComboBox *m_comboBorderSource = nullptr;
    DarkComboBox *m_comboTextColor = nullptr;

    DarkComboBox *m_comboBackdropType = nullptr;
    ResettableSlider *m_sldBackdropDim = nullptr;
    QCheckBox *m_chkPauseFocusLoss = nullptr;

    QCheckBox *m_chkAnimEnabled = nullptr;
    ResettableSlider *m_sldAnimSpeed = nullptr;
    DarkComboBox *m_comboAnimStyle = nullptr;
    QCheckBox *m_chkCursorGlow = nullptr;

    ResettableSlider *m_sldUiScale = nullptr;
    ResettableSlider *m_sldFontSize = nullptr;
    DarkComboBox *m_comboFontBold = nullptr;
    DarkComboBox *m_comboUiDensity = nullptr;

    DarkComboBox *m_comboSidebarPos = nullptr;
    ResettableSlider *m_sldSidebarWidth = nullptr;
    QCheckBox *m_chkSidebarLabels = nullptr;
    QCheckBox *m_chkRememberWinSize = nullptr;
    QCheckBox *m_chkStartMinimized = nullptr;
    QCheckBox *m_chkAlwaysOnTop = nullptr;

    QCheckBox *m_chkHighContrast = nullptr;
    QCheckBox *m_chkReduceTransp = nullptr;
    DarkComboBox *m_comboEffectsQuality = nullptr;
    QCheckBox *m_chkDisableBlurGpu = nullptr;

    void applyLayoutAndTypographyLive();
    void resetAppearanceToDefault();
    QTextEdit *m_logView = nullptr;

    QLabel *m_lblGitVersionInfo = nullptr;
    QProgressBar *m_updateProgressBar = nullptr;
    QTextEdit *m_updateLogView = nullptr;

    QVariantAnimation *m_sidebarAnim = nullptr;
    QTimer *m_hoverTimer = nullptr;
    QTimer *m_autosaveTimer = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;

    QQueue<QString> m_toastQueue;
    bool m_toastActive = false;
    bool m_isLoadingUi = true;
    bool m_sidebarExpanded = true;
    bool m_pendingHover = true;
    bool m_forceQuit = false;
    bool m_syncCookieGuard = false;
};

} // namespace mqblox
