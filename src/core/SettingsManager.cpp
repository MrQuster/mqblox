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
#include <algorithm>

namespace mqblox {

static bool writeFileIfChanged(const QString &filePath, const QByteArray &newBytes, QString *errMsg = nullptr) {
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

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
    if (sf.write(newBytes) != newBytes.size()) {
        if (errMsg) *errMsg = "Ошибка записи файла: " + filePath;
        return false;
    }
    if (!sf.commit()) {
        if (errMsg) *errMsg = "Ошибка атомарного сохранения файла: " + filePath;
        return false;
    }
    return true;
}

SettingsManager::SettingsManager() {
    QDir().mkpath(mqDir());
    QDir().mkpath(accountsDir());
    QDir().mkpath(mqDir() + "/profiles");
    QDir().mkpath(macobloxDataDir());
    m_accounts.push_back(AccountProfile{});
}

QString SettingsManager::mqDir() const {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    if (!testDir.isEmpty()) return testDir + "/mqblox";
    return QDir::homePath() + "/.local/share/mqblox";
}

QString SettingsManager::accountsDir() const {
    return mqDir() + "/accounts";
}

QString SettingsManager::macobloxDataDir() const {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    if (!testDir.isEmpty()) return testDir + "/macoblox";
    const QString xdgData = qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share");
    const QString localDir = xdgData + "/macoblox";
    if (QFileInfo::exists(localDir + "/RobloxPlayer.app/Contents/MacOS/RobloxPlayer")) {
        return localDir;
    }
    const QString flatpakDir = QDir::homePath() + "/.var/app/xyz.narez.MacOBlox/data/macoblox";
    if (QFileInfo::exists(flatpakDir + "/RobloxPlayer.app/Contents/MacOS/RobloxPlayer")) {
        return flatpakDir;
    }
    return localDir;
}

QString SettingsManager::robloxAppDir() const {
    return macobloxDataDir() + "/RobloxPlayer.app";
}

QString SettingsManager::clientAppSettingsPath() const {
    return robloxAppDir() + "/Contents/MacOS/ClientSettings/ClientAppSettings.json";
}

QString SettingsManager::darlingPrefixDir() const {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    if (!testDir.isEmpty()) return testDir + "/darling";
    const QString envPrefix = qEnvironmentVariable("DPREFIX");
    if (!envPrefix.isEmpty()) return envPrefix;
    const QString homePrefix = QDir::homePath() + "/.darling";
    if (QDir(homePrefix).exists()) return homePrefix;
    const QString flatpakPrefix = QDir::homePath() + "/.var/app/xyz.narez.MacOBlox/data/darling";
    if (QDir(flatpakPrefix).exists()) return flatpakPrefix;
    return homePrefix;
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
    if (s.startsWith("roblox://", Qt::CaseInsensitive) ||
        s.startsWith("roblox-player:", Qt::CaseInsensitive)) {
        return s;
    }

    static const QRegularExpression rxDigits("^\\d+$");
    if (rxDigits.match(s).hasMatch()) {
        return "roblox://experiences/start?placeId=" + s;
    }
    static const QRegularExpression rxUrl(
        "roblox\\.com/(?:[a-z]{2}/)?games/(\\d+)(?:[^?]*\\?[^#]*privateServerLinkCode=([A-Za-z0-9_-]+))?",
        QRegularExpression::CaseInsensitiveOption);
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
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    const QString cfgDir = testDir.isEmpty() ? (QDir::homePath() + "/.config/mqblox") : (testDir + "/config");
    const QString cfgV36 = cfgDir + "/settings_v36.json";
    const QString cfgV35 = cfgDir + "/settings_v35.json";
    const QString cfgV34 = cfgDir + "/settings_v34.json";
    const QString legacyV13 = mqDir() + "/settings_v13.json";

    QString loadPath = legacyV13;
    if (QFileInfo::exists(cfgV36)) loadPath = cfgV36;
    else if (QFileInfo::exists(cfgV35)) loadPath = cfgV35;
    else if (QFileInfo::exists(cfgV34)) loadPath = cfgV34;

    QFile f(loadPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonParseError perr{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject root = doc.object();
    const int fileSchema = root.value("schemaVersion").toInt(0);
    m_activeAccountId = root.value("activeAccountId").toString("default");
    QJsonArray arr = root.value("accounts").toArray();
    if (!arr.isEmpty()) {
        m_accounts.clear();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            AccountProfile p;
            p.id = o.value("id").toString("default");
            p.label = o.value("label").toString("Main");
            p.displayName = o.value("displayName").toString(p.label);
            if (p.displayName.trimmed().isEmpty()) p.displayName = p.label;
            m_accounts.push_back(p);
        }
    }

    QJsonObject s = root.value("settings").toObject();
    if (s.isEmpty() && root.contains("targetFps")) {
        // Legacy flat root schema (v13)
        s = root;
    }
    if (!s.isEmpty()) {
        SettingsRegistry::readFromJson(m_settings, s);
    }

    if (loadPath != cfgV36 || fileSchema != SettingsRegistry::kCurrentSchemaVersion) {
        saveAll();
    }
}

bool SettingsManager::saveAll(QString *errMsg) {
    if (!writeClientAppSettingsJson(errMsg)) return false;

    QJsonObject s = SettingsRegistry::writeToJson(m_settings);

    QJsonArray arr;
    for (const auto &acc : m_accounts) {
        QJsonObject o;
        o["id"] = acc.id;
        o["label"] = acc.label;
        o["displayName"] = acc.displayName;
        arr.append(o);
    }

    QJsonObject root;
    root["schemaVersion"] = SettingsRegistry::kCurrentSchemaVersion;
    root["activeAccountId"] = m_activeAccountId;
    root["accounts"] = arr;
    root["settings"] = s;

    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    const QString cfgDir = testDir.isEmpty() ? (QDir::homePath() + "/.config/mqblox") : (testDir + "/config");
    QDir().mkpath(cfgDir);
    for (const QString &target : {
             cfgDir + "/settings_v36.json",
             mqDir() + "/settings_v13.json"}) {
        if (!writeFileIfChanged(target, payload, errMsg)) {
            return false;
        }
    }
    return true;
}

QJsonObject SettingsManager::buildEffectiveFflags(QString *errMsg) const {
    QString rawCustom = m_settings.customFflagsJson.trimmed();
    if (rawCustom.isEmpty()) rawCustom = "{}";

    QJsonParseError perr{};
    QJsonDocument customDoc = QJsonDocument::fromJson(rawCustom.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !customDoc.isObject()) {
        if (errMsg) *errMsg = "Ошибка синтаксиса JSON в FastFlags: " + perr.errorString();
        return {};
    }

    QJsonObject fflags;
    const QJsonObject userCustomObj = customDoc.object();
    for (auto it = userCustomObj.begin(); it != userCustomObj.end(); ++it) {
        fflags[it.key()] = it.value();
    }

    const int fpsVal = m_settings.unlimitedFps ? 9999 : m_settings.targetFps;
    fflags["DFIntTaskSchedulerTargetFps"] = fpsVal;
    fflags["FFlagTaskSchedulerLimitTargetFpsTo2402"] = false;
    fflags["FFlagGameBasicSettingsFramerateCap5"] = true;

    if (m_settings.graphicsBackend == GraphicsBackend::Vulkan) {
        fflags["FFlagDebugGraphicsPreferVulkan"] = true;
        fflags["FFlagDebugGraphicsDisableVulkan"] = false;
        fflags["FFlagDebugGraphicsPreferOpenGL"] = false;
        fflags["FFlagDebugGraphicsDisableOpenGL"] = true;
    } else {
        fflags["FFlagDebugGraphicsPreferOpenGL"] = true;
        fflags["FFlagDebugGraphicsDisableOpenGL"] = false;
        fflags["FFlagDebugGraphicsPreferVulkan"] = false;
        fflags["FFlagDebugGraphicsDisableVulkan"] = true;
    }

    if (m_settings.lighting == LightingMode::Voxel) {
        // Modern Roblox (0.744+) removed legacy Voxel shaders, causing missing shader crashes.
        // Emulate Voxel lighting via ShadowMap with zeroed shadows and culling for maximum FPS.
        fflags["FFlagDebugForceFutureIsBrightPhase2"] = true;
        fflags["FIntRenderShadowIntensity"] = 0;
        fflags["DFIntCullFactorPixelThresholdShadowMapHighQuality"] = 2147483647;
        fflags["DFIntCullFactorPixelThresholdShadowMapLowQuality"] = 2147483647;
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

    if (m_settings.disableTelemetry) {
        fflags["FFlagDebugDisableTelemetryEphemeralCounter"] = true;
        fflags["FFlagDebugDisableTelemetryEphemeralStat"] = true;
        fflags["FFlagDebugDisableTelemetryEventIngest"] = true;
        fflags["FFlagDebugDisableTelemetryPoint"] = true;
        fflags["FFlagDebugDisableTelemetryV2Counter"] = true;
        fflags["FFlagDebugDisableTelemetryV2Event"] = true;
        fflags["FFlagDebugDisableTelemetryV2Stat"] = true;
    }

    return fflags;
}

bool SettingsManager::writeClientAppSettingsJson(QString *errMsg) const {
    QString err;
    const QJsonObject managedFlags = buildEffectiveFflags(&err);
    if (!err.isEmpty()) {
        if (errMsg) *errMsg = err;
        return false;
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
        "FIntCameraMaxZoomDistance", "DFIntCameraMaxZoomDistance",
        "FIntFRMMinGrassDistance", "FIntFRMMaxGrassDistance", "FIntRenderGrassDetailStrands",
        "DFIntConnectionMTUSize", "FIntRakNetResendBufferArrayLength", "FFlagDebugDisplayFPS",
        "FFlagDebugDisableTelemetryEphemeralCounter", "FFlagDebugDisableTelemetryEphemeralStat",
        "FFlagDebugDisableTelemetryEventIngest", "FFlagDebugDisableTelemetryPoint",
        "FFlagDebugDisableTelemetryV2Counter", "FFlagDebugDisableTelemetryV2Event",
        "FFlagDebugDisableTelemetryV2Stat"
    };

    // 1. Write MacOBlox's RobloxPlayer.app/Contents/MacOS/ClientSettings/ClientAppSettings.json
    const QString clientSettingsFile = clientAppSettingsPath();
    QJsonObject clientObj;
    {
        QFile oldF(clientSettingsFile);
        if (oldF.open(QIODevice::ReadOnly)) {
            QJsonParseError oldErr{};
            QJsonDocument oldDoc = QJsonDocument::fromJson(oldF.readAll(), &oldErr);
            if (oldErr.error == QJsonParseError::NoError && oldDoc.isObject()) {
                clientObj = oldDoc.object();
            }
        }
    }
    for (const QString &k : kManagedKeys) {
        clientObj.remove(k);
    }
    for (auto it = managedFlags.begin(); it != managedFlags.end(); ++it) {
        clientObj[it.key()] = it.value();
    }
    if (!writeFileIfChanged(clientSettingsFile,
                            QJsonDocument(clientObj).toJson(QJsonDocument::Indented),
                            errMsg)) {
        return false;
    }

    return true;
}

bool SettingsManager::exportPresetToFile(const QString &filePath) const {
    QJsonObject root;
    root["schemaVersion"] = SettingsRegistry::kCurrentSchemaVersion;
    root["settings"] = SettingsRegistry::writeToJson(m_settings);
    return writeFileIfChanged(filePath, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool SettingsManager::importPresetFromFile(const QString &filePath, QString *errMsg) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errMsg) *errMsg = "Не удалось открыть файл профиля: " + filePath;
        return false;
    }
    QJsonParseError perr{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errMsg) *errMsg = "Некорректный JSON профиля: " + perr.errorString();
        return false;
    }
    QJsonObject root = doc.object();
    QJsonObject sObj = root.contains("settings") ? root.value("settings").toObject() : root;
    SettingsRegistry::readFromJson(m_settings, sObj);
    return saveAll(errMsg);
}

QStringList SettingsManager::listNamedProfiles() const {
    QDir d(mqDir() + "/profiles");
    QStringList out;
    for (const QFileInfo &fi : d.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        out.append(fi.completeBaseName());
    }
    return out;
}

bool SettingsManager::saveNamedProfile(const QString &name, QString *errMsg) const {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(R"([^a-zA-Z0-9_\-\x{0400}-\x{04FF} ])"), "_");
    clean = clean.simplified();
    if (clean.isEmpty()) {
        if (errMsg) *errMsg = "Имя профиля не может быть пустым";
        return false;
    }
    QJsonObject root;
    root["schemaVersion"] = SettingsRegistry::kCurrentSchemaVersion;
    root["settings"] = SettingsRegistry::writeToJson(m_settings);
    return writeFileIfChanged(mqDir() + "/profiles/" + clean + ".json",
                              QJsonDocument(root).toJson(QJsonDocument::Indented),
                              errMsg);
}

bool SettingsManager::loadNamedProfile(const QString &name, QString *errMsg) {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(R"([^a-zA-Z0-9_\-\x{0400}-\x{04FF} ])"), "_");
    clean = clean.simplified();
    return importPresetFromFile(mqDir() + "/profiles/" + clean + ".json", errMsg);
}

bool SettingsManager::deleteNamedProfile(const QString &name) const {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(R"([^a-zA-Z0-9_\-\x{0400}-\x{04FF} ])"), "_");
    clean = clean.simplified();
    return QFile::remove(mqDir() + "/profiles/" + clean + ".json");
}

} // namespace mqblox
