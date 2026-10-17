#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace mqblox {

enum class ResolutionMode { Auto = 0, HD_720p = 1, FHD_1080p = 2, QHD_2K = 3 };
enum class GpuVendorMode { Auto = 0, AMD = 1, Nvidia = 2, Intel = 3 };
enum class CpuVendorMode { Auto = 0, Intel = 1, AMD = 2 };
enum class WindowDriverMode { AutoPassthrough = 0, ForceX11 = 1, ForceWayland = 2 };
enum class GraphicsApi { OpenGL = 0, Vulkan = 1 };
enum class LightingMode { Voxel = 0, ShadowMap = 1, FuturePhase3 = 2, GameDefault = 3 };
enum class TextureQuality { Auto = 0, Level0 = 1, Level1 = 2, Level2 = 3, Level3 = 4 };
enum class MsaaMode { Off = 0, X2 = 1, X4 = 2, X8 = 3 };
enum class CameraZoomMode { Default = 0, Medium500 = 1, Far2000 = 2, Cosmic9999 = 3 };
enum class NetworkProfile { UltraLowPing = 0, Balanced = 1 };

struct AccountProfile {
    QString id = "default";
    QString label = "Main";
    QString username;
    QString displayName = "MrQuster";
    qulonglong userId = 0;
    QString robux = "—";
};

struct AppSettings {
    int schemaVersion = 34;

    ResolutionMode resMode = ResolutionMode::Auto;
    GpuVendorMode gpuVendor = GpuVendorMode::Auto;
    CpuVendorMode cpuVendor = CpuVendorMode::Auto;
    WindowDriverMode windowDriver = WindowDriverMode::AutoPassthrough;
    GraphicsApi graphicsApi = GraphicsApi::Vulkan;
    QString optMode = "performance";
    LightingMode lighting = LightingMode::Voxel;
    TextureQuality textureQuality = TextureQuality::Auto;
    MsaaMode msaa = MsaaMode::Off;
    CameraZoomMode camZoom = CameraZoomMode::Cosmic9999;
    NetworkProfile netProfile = NetworkProfile::UltraLowPing;

    int targetFps = 240;
    bool unlimitedFps = false;
    int frmQualityLevel = 0;

    QString audioSink = "default";
    QString audioSource = "default";

    bool returnToMqBloxOnLeave = true;
    bool pauseWebInGame = true;
    bool animBg = true;
    bool closeToTray = true;
    bool glThread = true;
    bool asyncShaders = true;
    bool vsyncOff = true;
    bool pipewireBuffer = true;
    bool mallocTrim = true;
    bool smartAutoCleanCache = true;
    bool lowRamFflags = false;
    bool disableTelemetry = true;
    bool enableGamemode = true;
    bool autoLaunchFromBrowser = true;
    bool autoReconnectOnCrash = false;
    bool hideSoberDesktop = false;
    bool sidebarPinned = true;

    bool noShadows = false;
    bool ultraShadows = false;
    bool noPostFx = false;
    bool lowMeshes = false;
    bool greySky = false;
    bool bringBackOof = true;
    bool discordRpc = true;
    bool serverRegionIndicator = true;
    bool gamepadSupport = true;
    bool enableHiDpi = false;

    // Игровые твики
    bool showHudFps = false;
    bool clearFog = false;
    int customMaxZoom = 99999;
    bool unlockInfiniteZoom = true;
    bool noclipCamWalls = false;
    bool fullBright = false;
    bool showHitboxes = false;
    bool wallhopPhysics = false;

    // Настройки внешнего вида HyperOS 4 Liquid Glass (v34.0)
    int glassBlurStrength = 4;     // 2 = Низкое, 4 = Среднее, 7 = Высокое
    int glassOpacityPct = 10;      // Прозрачность подложки (4..35%)
    int borderBrightnessPct = 100; // Яркость градиентной рамки 1px (40..150%)
    bool chameleonEnabled = true;  // Адаптация цвета рамок под видеофон
    bool reducedMotion = false;    // Режим «Меньше анимаций»
    int videoFpsLimit = 30;        // 15, 24 или 30 FPS
    int accentPreset = 0;          // 0=Голубой, 1=Золотой, 2=Изумрудный, 3=Фиолетовый

    QString lastPlaceInput;
    QString customFflagsJson = "{}";
    QStringList leaveMarkers = {"leaveGame", "Disconnect", "GameShutDown"};
};

} // namespace mqblox
