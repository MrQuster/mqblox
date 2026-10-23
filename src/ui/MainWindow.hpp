#pragma once
#include "core/SettingsManager.hpp"
#include "core/DarlingController.hpp"
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
#include <vector>

namespace mqblox {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &launchUri = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;

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
    void refreshAccountsUi();
    void refreshEnvironmentAndSessionUi();
    void refreshNamedProfilesCombo();
    void applyResolutionMode(ResolutionMode mode, bool initial);
    void applyGraphicsMasterPreset(int presetIdx);
    void toggleSidebarPinMode();
    void animateSidebar(bool expand);
    void switchTab(int idx);
    void runWatchdogTick();
    void refreshGitUpdateStatus();

    struct SearchEntry {
        int tabIndex = 0;
        QFrame *card = nullptr;
        QString haystackLower;
    };
    std::vector<SearchEntry> m_searchIndex;
    QTimer *m_searchDebounce = nullptr;
    QString m_lastAppliedQss;
    void buildSearchIndex();
    void filterSettingsBySearch(const QString &query);
    void resetCurrentCategoryToDefaults();
    void applyLayoutAndTypographyLive();
    void resetAppearanceToDefault();

    QString m_launchUri;
    SettingsManager m_settingsMgr;
    DarlingController *m_darlingCtrl = nullptr;

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

    // Home: Launch, Session, Wizard & Diagnostics
    QLineEdit *m_inpPlace = nullptr;
    QLabel *m_lblSessionStatus = nullptr;
    AnimButton *m_btnLogout = nullptr;
    QLabel *m_lblEnvSummary = nullptr;
    QProgressBar *m_wizardProgressBar = nullptr;
    QLabel *m_lblWizardStatus = nullptr;
    AnimButton *m_btnRunWizard = nullptr;
    QTextEdit *m_homeDiagView = nullptr;

    // Accounts & Named Profiles
    QVBoxLayout *m_accountsListLayout = nullptr;
    DarkComboBox *m_comboNamedProfiles = nullptr;
    QLineEdit *m_inpNewProfileName = nullptr;

    // System & MacOBlox Runtime
    QLabel *m_hwBannerLbl = nullptr;
    ResettableSlider *m_sldMouseSens = nullptr;
    QCheckBox *m_chkHideMenuBar = nullptr;
    DarkComboBox *m_comboDnsProvider = nullptr;
    QLineEdit *m_inpDnsCustom = nullptr;
    QCheckBox *m_chkFpsLog = nullptr;
    QCheckBox *m_chkTraceUdp = nullptr;
    QCheckBox *m_chkTraceLock = nullptr;
    QCheckBox *m_chkTraceGl = nullptr;
    QCheckBox *m_chkDiagSignals = nullptr;

    QCheckBox *m_chkReturnMq = nullptr;
    QCheckBox *m_chkAnimBg = nullptr;
    QCheckBox *m_chkCloseToTray = nullptr;
    QCheckBox *m_chkVsyncOff = nullptr;
    QCheckBox *m_chkAutoClean = nullptr;
    QCheckBox *m_chkMemFflags = nullptr;
    QCheckBox *m_chkNoTelemetry = nullptr;
    QCheckBox *m_chkAutoReconnect = nullptr;

    // Graphics & FastFlags
    QLabel *m_lblPresetDesc = nullptr;
    DarkComboBox *m_comboGfxBackend = nullptr;
    QLabel *m_lblGfxBackendHint = nullptr;
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

    QTextEdit *m_fflagsEditor = nullptr;
    QLabel *m_lblFflagsValidation = nullptr;

    // Tweaks & Camera
    DarkComboBox *m_comboCamZoom = nullptr;
    DarkComboBox *m_comboNetOpt = nullptr;
    QCheckBox *m_chkHudFps = nullptr;
    QCheckBox *m_chkClearFog = nullptr;
    QSpinBox *m_spinMaxZoom = nullptr;
    QCheckBox *m_chkInfZoom = nullptr;
    QCheckBox *m_chkFullBright = nullptr;
    QCheckBox *m_chkHitboxes = nullptr;

    // Visual & Layout Controls (HyperOS 4 Liquid Glass)
    QLineEdit *m_inpSearchSettings = nullptr;
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
    DarkComboBox *m_comboAccent = nullptr;
    QLineEdit *m_inpCustomHex = nullptr;
    QCheckBox *m_chkChameleon = nullptr;
    ResettableSlider *m_sldChamSpeed = nullptr;
    ResettableSlider *m_sldChamStrength = nullptr;
    DarkComboBox *m_comboBorderSource = nullptr;
    DarkComboBox *m_comboTextColor = nullptr;

    DarkComboBox *m_comboBackdropType = nullptr;
    ResettableSlider *m_sldBackdropDim = nullptr;
    DarkComboBox *m_comboVideoFps = nullptr;
    QCheckBox *m_chkPauseFocusLoss = nullptr;

    QCheckBox *m_chkAnimEnabled = nullptr;
    ResettableSlider *m_sldAnimSpeed = nullptr;
    DarkComboBox *m_comboAnimStyle = nullptr;
    QCheckBox *m_chkCursorGlow = nullptr;
    QCheckBox *m_chkReducedMotion = nullptr;

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

    QTextEdit *m_logView = nullptr;

    QLabel *m_lblGitVersionInfo = nullptr;
    QProgressBar *m_updateProgressBar = nullptr;
    QTextEdit *m_updateLogView = nullptr;

    QVariantAnimation *m_sidebarAnim = nullptr;
    QTimer *m_hoverTimer = nullptr;
    QTimer *m_autosaveTimer = nullptr;
    QTimer *m_watchdogTimer = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;

    QQueue<QString> m_toastQueue;
    bool m_toastActive = false;
    bool m_isLoadingUi = true;
    bool m_sidebarExpanded = true;
    bool m_pendingHover = true;
    bool m_forceQuit = false;
    int m_consecutiveCrashCount = 0;
    qint64 m_lastCrashTimestamp = 0;
};

} // namespace mqblox
