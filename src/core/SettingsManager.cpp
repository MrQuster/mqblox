#include "core/SettingsManager.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace mqblox {

SettingsManager::SettingsManager() {
    QDir().mkpath(mqDir());
    QDir().mkpath(accountsDir());
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
    static const QRegularExpression rxUrl("roblox\\.com/(?:[a-z]{2}/)?games/(\\d+)");
    auto m = rxUrl.match(s);
    if (m.hasMatch()) {
        return "roblox://experiences/start?placeId=" + m.captured(1);
    }
    return {};
}

void SettingsManager::loadAll() {
    QFile f(mqDir() + "/settings_v13.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

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
    m_settings.resMode = static_cast<ResolutionMode>(s.value("resMode").toInt(0));
    m_settings.gpuVendor = static_cast<GpuVendorMode>(s.value("gpuVendor").toInt(0));
    m_settings.cpuVendor = static_cast<CpuVendorMode>(s.value("cpuVendor").toInt(0));
    m_settings.windowDriver = static_cast<WindowDriverMode>(s.value("windowDriver").toInt(0));
    m_settings.graphicsApi = static_cast<GraphicsApi>(s.value("graphicsApi").toInt(1));
    m_settings.optMode = s.value("optMode").toString("performance");
    m_settings.lighting = static_cast<LightingMode>(s.value("lighting").toInt(0));
    m_settings.textureQuality = static_cast<TextureQuality>(s.value("textureQuality").toInt(0));
    m_settings.msaa = static_cast<MsaaMode>(s.value("msaa").toInt(0));
    m_settings.camZoom = static_cast<CameraZoomMode>(s.value("camZoom").toInt(0));
    m_settings.netProfile = static_cast<NetworkProfile>(s.value("netProfile").toInt(0));

    m_settings.targetFps = s.value("targetFps").toInt(240);
    m_settings.unlimitedFps = s.value("unlimitedFps").toBool(false);
    m_settings.frmQualityLevel = s.value("frmQualityLevel").toInt(0);

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

    m_settings.lastPlaceInput = s.value("lastPlaceInput").toString();
    m_settings.customFflagsJson = s.value("customFflagsJson").toString("{}");
}

bool SettingsManager::saveAll(QString *errMsg) {
    if (!writeSoberConfigJson(errMsg)) return false;

    QJsonObject s;
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
    root["activeAccountId"] = m_activeAccountId;
    root["accounts"] = arr;
    root["settings"] = s;

    QFile f(mqDir() + "/settings_v13.json");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
    return true;
}

bool SettingsManager::writeSoberConfigJson(QString *errMsg) const {
    QString rawCustom = m_settings.customFflagsJson.trimmed();
    if (rawCustom.isEmpty()) rawCustom = "{}";

    QJsonParseError perr;
    QJsonDocument customDoc = QJsonDocument::fromJson(rawCustom.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !customDoc.isObject()) {
        if (errMsg) *errMsg = "Ошибка JSON в FastFlags: " + perr.errorString();
        return false;
    }

    QJsonObject fflags = customDoc.object();
    fflags["DFIntTaskSchedulerTargetFps"] = m_settings.unlimitedFps ? 9999 : m_settings.targetFps;

    if (m_settings.graphicsApi == GraphicsApi::Vulkan) {
        fflags["FFlagDebugGraphicsPreferVulkan"] = true;
        fflags["FFlagDebugGraphicsDisableOpenGL"] = true;
    } else {
        fflags["FFlagDebugGraphicsPreferOpenGL"] = true;
        fflags["FFlagDebugGraphicsPreferVulkan"] = false;
    }

    if (m_settings.lighting == LightingMode::Voxel) {
        fflags["DFFlagDebugRenderForceTechnologyVoxel"] = true;
    } else if (m_settings.lighting == LightingMode::ShadowMap) {
        fflags["FFlagDebugForceFutureIsBrightPhase2"] = true;
    } else if (m_settings.lighting == LightingMode::FuturePhase3) {
        fflags["FFlagDebugForceFutureIsBrightPhase3"] = true;
    }

    if (m_settings.noShadows) fflags["FIntRenderShadowIntensity"] = 0;
    if (m_settings.ultraShadows) fflags["FIntRenderShadowMapBias"] = 0;
    if (m_settings.noPostFx) fflags["FFlagDisablePostFx"] = true;
    if (m_settings.greySky) fflags["FFlagDebugSkyGray"] = true;
    if (m_settings.frmQualityLevel > 0) fflags["DFIntDebugFRMQualityLevelOverride"] = m_settings.frmQualityLevel;

    QJsonObject soberCfg;
    soberCfg["bring_back_oof"] = m_settings.bringBackOof;
    soberCfg["discord_rpc_enabled"] = m_settings.discordRpc;
    soberCfg["enable_gamemode"] = m_settings.enableGamemode;
    soberCfg["enable_hidpi"] = m_settings.enableHiDpi;
    soberCfg["server_location_indicator_enabled"] = m_settings.serverRegionIndicator;
    soberCfg["use_console_experience"] = m_settings.gamepadSupport;
    soberCfg["disable_telemetry"] = m_settings.disableTelemetry;
    soberCfg["graphics_optimization_mode"] = m_settings.optMode;
    soberCfg["fflags"] = fflags;

    QString cfgDir = soberBaseDir() + "/config/sober";
    QDir().mkpath(cfgDir);
    QFile cf(cfgDir + "/config.json");
    if (cf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cf.write(QJsonDocument(soberCfg).toJson(QJsonDocument::Indented));
        cf.close();
    }
    return true;
}

bool SettingsManager::exportPresetToFile(const QString &filePath) const {
    QFile src(mqDir() + "/settings_v13.json");
    if (!src.exists()) return false;
    QFile::remove(filePath);
    return src.copy(filePath);
}

bool SettingsManager::importPresetFromFile(const QString &filePath, QString *errMsg) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errMsg) *errMsg = "Не удалось открыть файл пресета";
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errMsg) *errMsg = "Некорректный JSON файла пресета";
        return false;
    }
    QFile dst(mqDir() + "/settings_v13.json");
    if (dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        dst.write(doc.toJson(QJsonDocument::Indented));
        dst.close();
    }
    loadAll();
    return saveAll(errMsg);
}

} // namespace mqblox
