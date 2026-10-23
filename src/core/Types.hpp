#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace mqblox {

enum class SettingCategory {
    General = 0,
    System = 1,
    Graphics = 2,
    Tweaks = 3,
    Appearance = 4,
    Layout = 5,
    Accessibility = 6
};

enum class ResolutionMode { Auto = 0, HD_720p = 1, FHD_1080p = 2, QHD_2K = 3 };
enum class LightingMode { Voxel = 0, ShadowMap = 1, FuturePhase3 = 2, GameDefault = 3 };
enum class TextureQuality { Auto = 0, Level0 = 1, Level1 = 2, Level2 = 3, Level3 = 4 };
enum class MsaaMode { Off = 0, X2 = 1, X4 = 2 };
enum class CameraZoomMode { Default = 0, Medium500 = 1, Far2000 = 2, Cosmic9999 = 3 };
enum class NetworkProfile { UltraLowPing = 0, Balanced = 1 };

struct AccountProfile {
    QString id = "default";
    QString label = "Main";
    QString displayName = "Main";
};

struct AppSettings {
    int schemaVersion = 36;

    ResolutionMode resMode = ResolutionMode::Auto;
    LightingMode lighting = LightingMode::Voxel;
    TextureQuality textureQuality = TextureQuality::Auto;
    MsaaMode msaa = MsaaMode::Off;
    CameraZoomMode camZoom = CameraZoomMode::Cosmic9999;
    NetworkProfile netProfile = NetworkProfile::UltraLowPing;

    int targetFps = 240;
    bool unlimitedFps = false;
    int frmQualityLevel = 0;

    // MacOBlox Runtime & Input (passed via MACOBLOX_* environment variables)
    int mouseSensitivityPct = 100; // 10..200 -> MACOBLOX_MOUSE_SENSITIVITY
    bool hideMenuBar = true;       // MACOBLOX_HIDE_MENU_BAR=0 when false
    int dnsProvider = 1;           // 0=System, 1=Quad9, 2=Cloudflare, 3=Google, 4=Custom DoT
    QString dnsCustom;             // Custom DoT IPv4 when dnsProvider == 4
    bool fpsLog = false;           // MACOBLOX_FPS_LOG=1
    bool traceUdp = false;         // MACOBLOX_TRACE_UDP=1
    bool traceLock = false;        // MACOBLOX_TRACE_LOCK=1
    bool traceGl = false;          // MACOBLOX_TRACE_GL=1
    bool diagnosticSignals = false;// MACOBLOX_DIAGNOSTIC_SIGNALS=1

    bool returnToMqBloxOnLeave = true;
    bool animBg = true;
    bool closeToTray = true;
    bool vsyncOff = true; // MACOBLOX_VSYNC=0 when true, MACOBLOX_VSYNC=1 when false
    bool smartAutoCleanCache = true;
    bool lowRamFflags = false;
    bool disableTelemetry = true;
    bool autoReconnectOnCrash = false;
    bool sidebarPinned = true;

    bool noShadows = false;
    bool ultraShadows = false;
    bool noPostFx = false;
    bool lowMeshes = false;
    bool greySky = false;

    bool showHudFps = false;
    bool clearFog = false;
    int customMaxZoom = 9999;
    bool unlockInfiniteZoom = true;
    bool fullBright = false;
    bool showHitboxes = false;

    // 1. Стекло (Liquid Glass)
    int glassBlurStrength = 4;
    int glassOpacityPct = 10;
    int borderThicknessPx = 10;
    int borderBrightnessPct = 100;
    int glassSaturationPct = 160;
    int specularStrengthPct = 100;
    int shadowStrengthPct = 100;
    int cornerRadiusMode = 1;
    int glassStyleMode = 0;

    // 2. Цвета и Хамелеон
    int themeMode = 0;
    int accentPreset = 0;
    QString customAccentHex = "#38BDF8";
    bool chameleonEnabled = true;
    int chameleonSpeedPct = 20;
    int chameleonStrengthPct = 100;
    int borderColorSource = 0;
    int textColorMode = 0;

    // 3. Фон
    int backdropType = 0;
    QString customBackdropPath;
    int backdropDimPct = 0;
    int videoFpsLimit = 30;
    bool pauseOnFocusLoss = false;

    // 4. Анимации
    bool animationsEnabled = true;
    int animSpeedPct = 100;
    int animStyle = 1;
    bool cursorGlowFollow = true;
    bool reducedMotion = false;

    // 5. Масштаб и текст
    int uiScalePct = 100;
    int fontSizePx = 13;
    int fontBoldness = 0;
    int uiDensity = 1;

    // 6. Раскладка и окно
    int sidebarPosition = 0;
    int sidebarWidthPx = 210;
    bool sidebarShowLabels = true;
    bool rememberWindowSize = true;
    int savedWindowWidth = 1120;
    int savedWindowHeight = 700;
    bool startMinimized = false;
    bool alwaysOnTop = false;

    // 7. Доступность и пресеты
    bool highContrast = false;
    bool reduceTransparency = false;
    int effectsQuality = 2;
    bool disableBlurLowGpu = false;
    int visualPreset = 0;

    QString lastPlaceInput;
    QString customFflagsJson = "{}";
    QStringList leaveMarkers = {"leaveGame", "Disconnect", "GameShutDown"};
};

} // namespace mqblox
