#include "core/SettingsManager.hpp"
#include "core/SettingsRegistry.hpp"
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

static bool writeFileIfChanged(const QString &filePath, const QByteArray &newBytes, QString *errMsg = nullptr) {
    QFile existing(filePath);
    if (existing.exists() && existing.open(QIODevice::ReadOnly)) {
        if (existing.readAll() == newBytes) {
            return true;
        }
        existing.close();
    }
    QSaveFile sf(filePath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errMsg) *errMsg = "Не удалось открыть файл для записи: " + filePath;
        return false;
    }
    sf.write(newBytes);
    if (!sf.commit()) {
        if (errMsg) *errMsg = "Ошибка атомарного сохранения файла: " + filePath;
        return false;
    }
    return true;
}


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
    QString cfgV35 = QDir::homePath() + "/.config/mqblox/settings_v35.json";
    QString cfgV34 = QDir::homePath() + "/.config/mqblox/settings_v34.json";
    QString legacyV13 = mqDir() + "/settings_v13.json";

    QString loadPath = legacyV13;
    if (QFileInfo::exists(cfgV35)) loadPath = cfgV35;
    else if (QFileInfo::exists(cfgV34)) loadPath = cfgV34;

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
    if (!s.isEmpty()) {
        SettingsRegistry::readFromJson(m_settings, s);
        if (root.value("schemaVersion").toInt(0) < 38) {
            m_settings.gamepadSupport = false;
            m_settings.resMode = ResolutionMode::Auto;
            m_settings.pipewireBuffer = false;
        }
    }
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

    QJsonObject s = SettingsRegistry::writeToJson(m_settings);

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
    root["schemaVersion"] = 38;
    root["activeAccountId"] = m_activeAccountId;
    root["accounts"] = arr;
    root["settings"] = s;

    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QString cfgDir = QDir::homePath() + "/.config/mqblox";
    QDir().mkpath(cfgDir);
    for (const QString &target : {
             cfgDir + "/settings_v35.json",
             mqDir() + "/settings_v13.json"}) {
        writeFileIfChanged(target, payload);
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

    if (QFileInfo::exists(cfgPath) && !QFileInfo::exists(bakPath)) {
        QFile::copy(cfgPath, bakPath);
    }

    QJsonObject soberCfg;
    QJsonObject existingFflags;
    {
        QFile oldF(cfgPath);
        if (oldF.open(QIODevice::ReadOnly)) {
            QJsonParseError oldErr;
            QJsonDocument oldDoc = QJsonDocument::fromJson(oldF.readAll(), &oldErr);
            if (oldErr.error == QJsonParseError::NoError && oldDoc.isObject()) {
                soberCfg = oldDoc.object();
                existingFflags = soberCfg.value("fflags").toObject();
            }
            oldF.close();
        }
    }

    static const QStringList kManagedKeys = {
        "DFIntTaskSchedulerTargetFps", "FFlagTaskSchedulerLimitTargetFpsTo2402",
        "FFlagGameBasicSettingsFramerateCap5", "FFlagDebugGraphicsPreferVulkan",
        "FFlagDebugGraphicsDisableOpenGL", "FFlagDebugGraphicsPreferOpenGL",
        "FFlagDebugGraphicsDisableVulkan", "DFFlagDebugRenderForceTechnologyVoxel",
        "FFlagDebugForceFutureIsBrightPhase2", "FFlagDebugForceFutureIsBrightPhase3",
        "DFFlagTextureQualityOverrideEnabled", "DFIntTextureQualityOverride",
        "FIntDebugForceMSAASamples", "DFIntDebugFRMQualityLevelOverride",
        "FIntRomarkStartWithGraphicQualityLevel", "FIntRenderShadowIntensity",
        "DFIntCullFactorPixelThresholdShadowMapHighQuality",
        "DFIntCullFactorPixelThresholdShadowMapLowQuality", "FIntRenderShadowMapBias",
        "FFlagDisablePostFx", "FFlagDebugSkyGray", "DFIntCSGLevelOfDetailSwitchingDistance",
        "DFIntCSGLevelOfDetailSwitchingDistanceL12", "DFIntCSGLevelOfDetailSwitchingDistanceL23",
        "DFIntCSGLevelOfDetailSwitchingDistanceL34", "DFIntHttpCacheCleanMinFilesRequired",
        "FIntRenderLocalLightUpdatesMax", "DFIntTextureCompositorActiveJobs",
        "FIntCameraMaxZoomDistance", "DFIntCameraMaxZoomDistance", "FIntCameraFarZPlane",
        "FIntPoppercamPassthrough", "DFIntCullFactorPixelThresholdMainViewHighQuality",
        "DFIntCullFactorPixelThresholdMainViewLowQuality", "FFlagDebugRenderingSetDeterministic",
        "DFIntS2PhysicsSenderRate", "DFIntMaxMissedWorldStepsRemembered",
        "FIntFRMMinGrassDistance", "FIntFRMMaxGrassDistance", "FIntRenderGrassDetailStrands",
        "FIntGrassMovementReducedMotionFactor", "DFIntConnectionMTUSize",
        "FIntRakNetResendBufferArrayLength", "FFlagDebugDisplayFPS", "DFIntRuntimeConcurrency",
        "FFlagDebugDisableTelemetryEphemeralCounter", "FFlagDebugDisableTelemetryEphemeralStat",
        "FFlagDebugDisableTelemetryEventIngest", "FFlagDebugDisableTelemetryPoint",
        "FFlagDebugDisableTelemetryV2Counter", "FFlagDebugDisableTelemetryV2Event",
        "FFlagDebugDisableTelemetryV2Stat"
    };
    for (const QString &k : kManagedKeys) {
        existingFflags.remove(k);
    }

    QJsonObject fflags = existingFflags;
    const QJsonObject userCustomObj = customDoc.object();
    for (auto it = userCustomObj.begin(); it != userCustomObj.end(); ++it) {
        fflags[it.key()] = it.value();
    }

    int fpsVal = m_settings.unlimitedFps ? 9999 : m_settings.targetFps;
    fflags["DFIntTaskSchedulerTargetFps"] = fpsVal;
    fflags["FFlagTaskSchedulerLimitTargetFpsTo2402"] = false;
    fflags["FFlagGameBasicSettingsFramerateCap5"] = true;

    if (m_settings.graphicsApi == GraphicsApi::Vulkan) {
        fflags["FFlagDebugGraphicsPreferVulkan"] = true;
        fflags["FFlagDebugGraphicsDisableOpenGL"] = true;
        fflags["FFlagDebugGraphicsPreferOpenGL"] = false;
        fflags["FFlagDebugGraphicsDisableVulkan"] = false;
    } else {
        fflags["FFlagDebugGraphicsPreferOpenGL"] = true;
        fflags["FFlagDebugGraphicsDisableOpenGL"] = false;
        fflags["FFlagDebugGraphicsPreferVulkan"] = false;
        fflags["FFlagDebugGraphicsDisableVulkan"] = true;
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
    } else if (m_settings.camZoom != CameraZoomMode::Default) {
        int presetZoom = 128;
        if (m_settings.camZoom == CameraZoomMode::Medium500) presetZoom = 500;
        else if (m_settings.camZoom == CameraZoomMode::Far2000) presetZoom = 2000;
        else if (m_settings.camZoom == CameraZoomMode::Cosmic9999) presetZoom = 99999;
        fflags["FIntCameraMaxZoomDistance"] = presetZoom;
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
    soberCfg["touch_mode"] = "off";
    soberCfg["disable_telemetry"] = m_settings.disableTelemetry;
    soberCfg["graphics_optimization_mode"] = m_settings.optMode;
    soberCfg["fflags"] = fflags;

    QByteArray jsonBytes = QJsonDocument(soberCfg).toJson(QJsonDocument::Indented);

    if (!writeFileIfChanged(cfgPath, jsonBytes, errMsg)) {
        return false;
    }

    QString clientSettingsDir = soberBaseDir() + "/data/sober/appData/ClientSettings";
    QDir().mkpath(clientSettingsDir);
    writeFileIfChanged(clientSettingsDir + "/ClientAppSettings.json",
                       QJsonDocument(fflags).toJson(QJsonDocument::Indented));
    return true;
}

bool SettingsManager::exportPresetToFile(const QString &filePath) const {
    QString srcPath = QDir::homePath() + "/.config/mqblox/settings_v35.json";
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
    for (const QString &dstPath : {cfgDir + "/settings_v35.json", mqDir() + "/settings_v13.json"}) {
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
