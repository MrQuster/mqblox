#include "core/SettingsManager.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThread>
#include <algorithm>

namespace mqblox {

SettingsManager::SettingsManager() {
    QDir().mkpath(mqDir());
    QDir().mkpath(accountsDir());
    QDir().mkpath(QDir::homePath() + "/.config/mqblox");
    m_accounts.push_back(AccountProfile{});
}

QString SettingsManager::mqDir() const {
    return QDir::homePath() + "/.local/share/mqblox";
}

QString SettingsManager::accountsDir() const {
    return mqDir() + "/accounts";
}

QString SettingsManager::soberBaseDir() const {
    return QDir::homePath() + "/.var/app/org.vinegarhq.Sober";
}

AccountProfile *SettingsManager::activeAccount() {
    for (auto &acc : m_accounts) {
        if (acc.id == m_activeAccountId) return &acc;
    }
    if (m_accounts.isEmpty()) m_accounts.push_back(AccountProfile{});
    m_activeAccountId = m_accounts.front().id;
    return &m_accounts.front();
}

QString SettingsManager::parsePlaceUri(const QString &input) {
    QString s = input.trimmed();
    if (s.isEmpty()) return {};
    if (s.startsWith("roblox://") || s.startsWith("roblox-player:")) return s;

    static const QRegularExpression rxDigits("^\\d+$");
    if (rxDigits.match(s).hasMatch()) {
        return "roblox://experiences/start?placeId=" + s;
    }
    static const QRegularExpression rxUrl(
        "roblox\\.com/(?:[a-z]{2}/)?games/(\\d+)(?:[^?]*\\?[^#]*privateServerLinkCode=([A-Za-z0-9_-]+))?");
    auto m = rxUrl.match(s);
    if (m.hasMatch()) {
        QString placeId = m.captured(1);
        QString linkCode = m.captured(2);
        if (!linkCode.isEmpty()) {
            return QString("roblox://experiences/start?placeId=%1&linkCode=%2").arg(placeId, linkCode);
        }
        return "roblox://experiences/start?placeId=" + placeId;
    }
    return {};
}

void SettingsManager::loadAll() {
    QString cfgV34 = QDir::homePath() + "/.config/mqblox/settings_v34.json";
    QString legacyV13 = mqDir() + "/settings_v13.json";

    QString loadPath = QFileInfo::exists(cfgV34) ? cfgV34 : legacyV13;
    QFile f(loadPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject root = doc.object();
    m_activeAccountId = root.value("activeAccountId").toString("default");
    QJsonArray arr = root.value("accounts").toArray();
    if (!arr.isEmpty()) {
        m_accounts.clear();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            AccountProfile p;
            p.id = o.value("id").toString("default");
            p.label = o.value("label").toString("Main");
            p.username = o.value("username").toString();
            p.displayName = o.value("displayName").toString(p.label);
            p.userId = o.value("userId").toString().toULongLong();
            p.robux = o.value("robux").toString("—");
            m_accounts.push_back(p);
        }
    }

    QJsonObject s = root.value("settings").toObject();
    if (s.isEmpty()) return;

    m_settings.schemaVersion = 34;
    m_settings.resMode = static_cast<ResolutionMode>(s.value("resMode").toInt(0));
    m_settings.gpuVendor = static_cast<GpuVendorMode>(s.value("gpuVendor").toInt(0));
    m_settings.cpuVendor = static_cast<CpuVendorMode>(s.value("cpuVendor").toInt(0));
    m_settings.windowDriver = static_cast<WindowDriverMode>(s.value("windowDriver").toInt(0));
    m_settings.graphicsApi = static_cast<GraphicsApi>(s.value("graphicsApi").toInt(1));
    m_settings.optMode = s.value("optMode").toString("performance");
    m_settings.lighting = static_cast<LightingMode>(s.value("lighting").toInt(0));
    m_settings.textureQuality = static_cast<TextureQuality>(s.value("textureQuality").toInt(0));
    m_settings.msaa = static_cast<MsaaMode>(s.value("msaa").toInt(0));
    m_settings.camZoom = static_cast<CameraZoomMode>(s.value("camZoom").toInt(3));
    m_settings.netProfile = static_cast<NetworkProfile>(s.value("netProfile").toInt(0));

    m_settings.targetFps = std::clamp(s.value("targetFps").toInt(240), 30, 9999);
    m_settings.unlimitedFps = s.value("unlimitedFps").toBool(false);
    m_settings.frmQualityLevel = std::clamp(s.value("frmQualityLevel").toInt(0), 0, 21);

    m_settings.returnToMqBloxOnLeave = s.value("returnToMqBloxOnLeave").toBool(true);
    m_settings.pauseWebInGame = s.value("pauseWebInGame").toBool(true);
    m_settings.animBg = s.value("animBg").toBool(true);
    m_settings.closeToTray = s.value("closeToTray").toBool(true);
    m_settings.glThread = s.value("glThread").toBool(true);
    m_settings.asyncShaders = s.value("asyncShaders").toBool(true);
    m_settings.vsyncOff = s.value("vsyncOff").toBool(true);
    m_settings.pipewireBuffer = s.value("pipewireBuffer").toBool(true);
    m_settings.mallocTrim = s.value("mallocTrim").toBool(true);
    m_settings.smartAutoCleanCache = s.value("smartAutoCleanCache").toBool(true);
    m_settings.lowRamFflags = s.value("lowRamFflags").toBool(false);
    m_settings.disableTelemetry = s.value("disableTelemetry").toBool(true);
    m_settings.enableGamemode = s.value("enableGamemode").toBool(true);
    m_settings.autoLaunchFromBrowser = s.value("autoLaunchFromBrowser").toBool(true);
    m_settings.autoReconnectOnCrash = s.value("autoReconnectOnCrash").toBool(false);
    m_settings.hideSoberDesktop = s.value("hideSoberDesktop").toBool(false);
    m_settings.sidebarPinned = s.value("sidebarPinned").toBool(true);

    m_settings.noShadows = s.value("noShadows").toBool(false);
    m_settings.ultraShadows = s.value("ultraShadows").toBool(false);
    m_settings.noPostFx = s.value("noPostFx").toBool(false);
    m_settings.lowMeshes = s.value("lowMeshes").toBool(false);
    m_settings.greySky = s.value("greySky").toBool(false);
    m_settings.bringBackOof = s.value("bringBackOof").toBool(true);
    m_settings.discordRpc = s.value("discordRpc").toBool(true);
    m_settings.serverRegionIndicator = s.value("serverRegionIndicator").toBool(true);
    m_settings.gamepadSupport = s.value("gamepadSupport").toBool(true);
    m_settings.enableHiDpi = s.value("enableHiDpi").toBool(false);

    m_settings.showHudFps = s.value("showHudFps").toBool(false);
    m_settings.clearFog = s.value("clearFog").toBool(false);
    m_settings.customMaxZoom = std::clamp(s.value("customMaxZoom").toInt(99999), 128, 100000);
    m_settings.unlockInfiniteZoom = s.value("unlockInfiniteZoom").toBool(true);
    m_settings.noclipCamWalls = s.value("noclipCamWalls").toBool(false);
    m_settings.fullBright = s.value("fullBright").toBool(false);
    m_settings.showHitboxes = s.value("showHitboxes").toBool(false);
    m_settings.wallhopPhysics = s.value("wallhopPhysics").toBool(false);

    m_settings.glassBlurStrength = std::clamp(s.value("glassBlurStrength").toInt(4), 1, 8);
    m_settings.glassOpacityPct = std::clamp(s.value("glassOpacityPct").toInt(10), 4, 40);
    m_settings.borderBrightnessPct = std::clamp(s.value("borderBrightnessPct").toInt(100), 30, 150);
    m_settings.chameleonEnabled = s.value("chameleonEnabled").toBool(true);
    m_settings.reducedMotion = s.value("reducedMotion").toBool(false);
    m_settings.videoFpsLimit = std::clamp(s.value("videoFpsLimit").toInt(30), 15, 60);
    m_settings.accentPreset = std::clamp(s.value("accentPreset").toInt(0), 0, 3);

    m_settings.lastPlaceInput = s.value("lastPlaceInput").toString();
    m_settings.customFflagsJson = s.value("customFflagsJson").toString("{}");
}

bool SettingsManager::saveAll(QString *errMsg) {
    if (!writeSoberConfigJson(errMsg)) return false;

    QString localApps = QDir::homePath() + "/.local/share/applications";
    QDir().mkpath(localApps);
    QString soberDesk = localApps + "/org.vinegarhq.Sober.desktop";
    if (m_settings.hideSoberDesktop) {
        QSaveFile sf(soberDesk);
        if (sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            sf.write("[Desktop Entry]\nType=Application\nName=Sober\nNoDisplay=true\n");
            sf.commit();
        }
    } else {
        QFile::remove(soberDesk);
    }

    QJsonObject s;
    s["schemaVersion"] = 34;
    s["resMode"] = static_cast<int>(m_settings.resMode);
    s["gpuVendor"] = static_cast<int>(m_settings.gpuVendor);
    s["cpuVendor"] = static_cast<int>(m_settings.cpuVendor);
    s["windowDriver"] = static_cast<int>(m_settings.windowDriver);
    s["graphicsApi"] = static_cast<int>(m_settings.graphicsApi);
    s["optMode"] = m_settings.optMode;
    s["lighting"] = static_cast<int>(m_settings.lighting);
    s["textureQuality"] = static_cast<int>(m_settings.textureQuality);
    s["msaa"] = static_cast<int>(m_settings.msaa);
    s["camZoom"] = static_cast<int>(m_settings.camZoom);
    s["netProfile"] = static_cast<int>(m_settings.netProfile);

    s["targetFps"] = m_settings.targetFps;
    s["unlimitedFps"] = m_settings.unlimitedFps;
    s["frmQualityLevel"] = m_settings.frmQualityLevel;

    s["returnToMqBloxOnLeave"] = m_settings.returnToMqBloxOnLeave;
    s["pauseWebInGame"] = m_settings.pauseWebInGame;
    s["animBg"] = m_settings.animBg;
    s["closeToTray"] = m_settings.closeToTray;
    s["glThread"] = m_settings.glThread;
    s["asyncShaders"] = m_settings.asyncShaders;
    s["vsyncOff"] = m_settings.vsyncOff;
    s["pipewireBuffer"] = m_settings.pipewireBuffer;
    s["mallocTrim"] = m_settings.mallocTrim;
    s["smartAutoCleanCache"] = m_settings.smartAutoCleanCache;
    s["lowRamFflags"] = m_settings.lowRamFflags;
    s["disableTelemetry"] = m_settings.disableTelemetry;
    s["enableGamemode"] = m_settings.enableGamemode;
    s["autoLaunchFromBrowser"] = m_settings.autoLaunchFromBrowser;
    s["autoReconnectOnCrash"] = m_settings.autoReconnectOnCrash;
    s["hideSoberDesktop"] = m_settings.hideSoberDesktop;
    s["sidebarPinned"] = m_settings.sidebarPinned;

    s["noShadows"] = m_settings.noShadows;
    s["ultraShadows"] = m_settings.ultraShadows;
    s["noPostFx"] = m_settings.noPostFx;
    s["lowMeshes"] = m_settings.lowMeshes;
    s["greySky"] = m_settings.greySky;
    s["bringBackOof"] = m_settings.bringBackOof;
    s["discordRpc"] = m_settings.discordRpc;
    s["serverRegionIndicator"] = m_settings.serverRegionIndicator;
    s["gamepadSupport"] = m_settings.gamepadSupport;
    s["enableHiDpi"] = m_settings.enableHiDpi;

    s["showHudFps"] = m_settings.showHudFps;
    s["clearFog"] = m_settings.clearFog;
    s["customMaxZoom"] = m_settings.customMaxZoom;
    s["unlockInfiniteZoom"] = m_settings.unlockInfiniteZoom;
    s["noclipCamWalls"] = m_settings.noclipCamWalls;
    s["fullBright"] = m_settings.fullBright;
    s["showHitboxes"] = m_settings.showHitboxes;
    s["wallhopPhysics"] = m_settings.wallhopPhysics;

    s["glassBlurStrength"] = m_settings.glassBlurStrength;
    s["glassOpacityPct"] = m_settings.glassOpacityPct;
    s["borderBrightnessPct"] = m_settings.borderBrightnessPct;
    s["chameleonEnabled"] = m_settings.chameleonEnabled;
    s["reducedMotion"] = m_settings.reducedMotion;
    s["videoFpsLimit"] = m_settings.videoFpsLimit;
    s["accentPreset"] = m_settings.accentPreset;

    s["lastPlaceInput"] = m_settings.lastPlaceInput;
    s["customFflagsJson"] = m_settings.customFflagsJson;

    QJsonArray arr;
    for (const auto &acc : m_accounts) {
        QJsonObject o;
        o["id"] = acc.id;
        o["label"] = acc.label;
        o["username"] = acc.username;
        o["displayName"] = acc.displayName;
        o["userId"] = QString::number(acc.userId);
        o["robux"] = acc.robux;
        arr.append(o);
    }

    QJsonObject root;
    root["schemaVersion"] = 34;
    root["activeAccountId"] = m_activeAccountId;
    root["accounts"] = arr;
    root["settings"] = s;

    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QString cfgDir = QDir::homePath() + "/.config/mqblox";
    QDir().mkpath(cfgDir);
    for (const QString &target : {cfgDir + "/settings_v34.json", mqDir() + "/settings_v13.json"}) {
        QSaveFile f(target);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(payload);
            f.commit();
        }
    }
    return true;
}

bool SettingsManager::writeSoberConfigJson(QString *errMsg) const {
    QString rawCustom = m_settings.customFflagsJson.trimmed();
    if (rawCustom.isEmpty()) rawCustom = "{}";

    QJsonParseError perr;
    QJsonDocument customDoc = QJsonDocument::fromJson(rawCustom.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !customDoc.isObject()) {
        if (errMsg) *errMsg = "Ошибка синтаксиса JSON в FastFlags: " + perr.errorString();
        return false;
    }

    QString cfgDir = soberBaseDir() + "/config/sober";
    QDir().mkpath(cfgDir);
    QString cfgPath = cfgDir + "/config.json";
    QString bakPath = cfgDir + "/config.json.mqblox.bak";

    // Создаём резервную копию оригинального config.json перед первой модификацией
    if (QFileInfo::exists(cfgPath) && !QFileInfo::exists(bakPath)) {
        QFile::copy(cfgPath, bakPath);
    }

    QJsonObject soberCfg;
    {
        QFile oldF(cfgPath);
        if (oldF.open(QIODevice::ReadOnly)) {
            QJsonParseError oldErr;
            QJsonDocument oldDoc = QJsonDocument::fromJson(oldF.readAll(), &oldErr);
            if (oldErr.error == QJsonParseError::NoError && oldDoc.isObject()) {
                soberCfg = oldDoc.object();
            }
            oldF.close();
        }
    }

    // Пользовательские FFlags из редактора имеют базовый приоритет, поверх которых записываются переключатели UI
    QJsonObject fflags = customDoc.object();

    int fpsVal = m_settings.unlimitedFps ? 9999 : m_settings.targetFps;
    fflags["DFIntTaskSchedulerTargetFps"] = fpsVal;
    fflags["FFlagTaskSchedulerLimitTargetFpsTo2402"] = false;
    fflags["FFlagGameBasicSettingsFramerateCap5"] = true;

    if (m_settings.graphicsApi == GraphicsApi::Vulkan) {
        fflags["FFlagDebugGraphicsPreferVulkan"] = true;
        fflags["FFlagDebugGraphicsDisableOpenGL"] = true;
    } else {
        fflags["FFlagDebugGraphicsPreferOpenGL"] = true;
        fflags["FFlagDebugGraphicsPreferVulkan"] = false;
    }

    if (m_settings.lighting == LightingMode::Voxel || m_settings.fullBright) {
        fflags["DFFlagDebugRenderForceTechnologyVoxel"] = true;
        fflags["FFlagDebugForceFutureIsBrightPhase2"] = false;
        fflags["FFlagDebugForceFutureIsBrightPhase3"] = false;
    } else if (m_settings.lighting == LightingMode::ShadowMap) {
        fflags["FFlagDebugForceFutureIsBrightPhase2"] = true;
    } else if (m_settings.lighting == LightingMode::FuturePhase3) {
        fflags["FFlagDebugForceFutureIsBrightPhase3"] = true;
    }

    if (m_settings.textureQuality != TextureQuality::Auto) {
        int texLvl = static_cast<int>(m_settings.textureQuality) - 1;
        fflags["DFFlagTextureQualityOverrideEnabled"] = true;
        fflags["DFIntTextureQualityOverride"] = texLvl;
    }

    int msaaSamples = 0;
    if (m_settings.msaa == MsaaMode::X2) msaaSamples = 2;
    else if (m_settings.msaa == MsaaMode::X4) msaaSamples = 4;
    else if (m_settings.msaa == MsaaMode::X8) msaaSamples = 8;
    if (msaaSamples > 0) {
        fflags["FIntDebugForceMSAASamples"] = msaaSamples;
    }

    if (m_settings.frmQualityLevel > 0) {
        fflags["DFIntDebugFRMQualityLevelOverride"] = m_settings.frmQualityLevel;
        fflags["FIntRomarkStartWithGraphicQualityLevel"] = m_settings.frmQualityLevel;
    }

    if (m_settings.noShadows || m_settings.fullBright) {
        fflags["FIntRenderShadowIntensity"] = 0;
        fflags["DFIntCullFactorPixelThresholdShadowMapHighQuality"] = 2147483647;
        fflags["DFIntCullFactorPixelThresholdShadowMapLowQuality"] = 2147483647;
    } else if (m_settings.ultraShadows) {
        fflags["FIntRenderShadowIntensity"] = 100;
        fflags["FIntRenderShadowMapBias"] = 0;
    }

    if (m_settings.noPostFx || m_settings.fullBright) {
        fflags["FFlagDisablePostFx"] = true;
    }
    if (m_settings.greySky) {
        fflags["FFlagDebugSkyGray"] = true;
    }

    if (m_settings.lowMeshes || m_settings.showHitboxes) {
        fflags["DFIntCSGLevelOfDetailSwitchingDistance"] = 0;
        fflags["DFIntCSGLevelOfDetailSwitchingDistanceL12"] = 0;
        fflags["DFIntCSGLevelOfDetailSwitchingDistanceL23"] = 0;
        fflags["DFIntCSGLevelOfDetailSwitchingDistanceL34"] = 0;
    }

    if (m_settings.lowRamFflags) {
        fflags["DFIntHttpCacheCleanMinFilesRequired"] = 300;
        fflags["FIntRenderLocalLightUpdatesMax"] = 1;
        fflags["DFIntTextureCompositorActiveJobs"] = 1;
    }

    if (m_settings.unlockInfiniteZoom) {
        fflags["FIntCameraMaxZoomDistance"] = m_settings.customMaxZoom;
    }

    if (m_settings.clearFog) {
        fflags["FIntFRMMinGrassDistance"] = 0;
        fflags["FIntFRMMaxGrassDistance"] = 0;
        fflags["FIntRenderGrassDetailStrands"] = 0;
    }

    if (m_settings.netProfile == NetworkProfile::UltraLowPing) {
        fflags["DFIntConnectionMTUSize"] = 1396;
        fflags["FIntRakNetResendBufferArrayLength"] = 128;
    } else {
        fflags["DFIntConnectionMTUSize"] = 1492;
    }

    if (m_settings.showHudFps) {
        fflags["FFlagDebugDisplayFPS"] = true;
    }

    int threads = std::max(2, QThread::idealThreadCount());
    if (m_settings.cpuVendor != CpuVendorMode::Auto) {
        fflags["DFIntRuntimeConcurrency"] = threads;
    }

    if (m_settings.disableTelemetry) {
        fflags["FFlagDebugDisableTelemetryEphemeralCounter"] = true;
        fflags["FFlagDebugDisableTelemetryEphemeralStat"] = true;
        fflags["FFlagDebugDisableTelemetryEventIngest"] = true;
        fflags["FFlagDebugDisableTelemetryPoint"] = true;
        fflags["FFlagDebugDisableTelemetryV2Counter"] = true;
        fflags["FFlagDebugDisableTelemetryV2Event"] = true;
        fflags["FFlagDebugDisableTelemetryV2Stat"] = true;
    }

    soberCfg["bring_back_oof"] = m_settings.bringBackOof;
    soberCfg["discord_rpc_enabled"] = m_settings.discordRpc;
    soberCfg["enable_gamemode"] = m_settings.enableGamemode;
    soberCfg["enable_hidpi"] = m_settings.enableHiDpi;
    soberCfg["server_location_indicator_enabled"] = m_settings.serverRegionIndicator;
    soberCfg["use_console_experience"] = m_settings.gamepadSupport;
    soberCfg["disable_telemetry"] = m_settings.disableTelemetry;
    soberCfg["graphics_optimization_mode"] = m_settings.optMode;
    soberCfg["fflags"] = fflags;

    QByteArray jsonBytes = QJsonDocument(soberCfg).toJson(QJsonDocument::Indented);

    QSaveFile cf(cfgPath);
    if (!cf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errMsg) *errMsg = "Не удалось открыть config.json для записи";
        return false;
    }
    cf.write(jsonBytes);
    if (!cf.commit()) {
        if (errMsg) *errMsg = "Ошибка атомарного сохранения config.json";
        return false;
    }

    QString clientSettingsDir = soberBaseDir() + "/data/sober/appData/ClientSettings";
    QDir().mkpath(clientSettingsDir);
    QSaveFile csf(clientSettingsDir + "/ClientAppSettings.json");
    if (csf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        csf.write(QJsonDocument(fflags).toJson(QJsonDocument::Indented));
        csf.commit();
    }
    return true;
}

bool SettingsManager::exportPresetToFile(const QString &filePath) const {
    QString srcPath = QDir::homePath() + "/.config/mqblox/settings_v34.json";
    if (!QFileInfo::exists(srcPath)) srcPath = mqDir() + "/settings_v13.json";
    QFile src(srcPath);
    if (!src.exists()) return false;
    QFile::remove(filePath);
    return src.copy(filePath);
}

bool SettingsManager::importPresetFromFile(const QString &filePath, QString *errMsg) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errMsg) *errMsg = "Не удалось открыть файл профиля";
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errMsg) *errMsg = "Некорректный JSON файла профиля";
        return false;
    }
    QString cfgDir = QDir::homePath() + "/.config/mqblox";
    QDir().mkpath(cfgDir);
    for (const QString &dstPath : {cfgDir + "/settings_v34.json", mqDir() + "/settings_v13.json"}) {
        QSaveFile dst(dstPath);
        if (dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            dst.write(doc.toJson(QJsonDocument::Indented));
            dst.commit();
        }
    }
    loadAll();
    return saveAll(errMsg);
}

} // namespace mqblox
