#include "core/SettingsManager.hpp"
#include "core/SettingsRegistry.hpp"
#include "core/SoberController.hpp"
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace mqblox;

class TestMqBlox : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_tmpDir.isValid());
        qputenv("MQBLOX_TEST_DIR", m_tmpDir.path().toUtf8());
    }

    void testPlaceUriParser() {
        QCOMPARE(SettingsManager::parsePlaceUri("1818"),
                 QString("roblox://experiences/start?placeId=1818"));
        QCOMPARE(SettingsManager::parsePlaceUri("https://www.roblox.com/games/2753915549/Blox-Fruits?privateServerLinkCode=ABC123xyz"),
                 QString("roblox://experiences/start?placeId=2753915549&linkCode=ABC123xyz"));
        QCOMPARE(SettingsManager::parsePlaceUri("invalid_text"), QString());
    }

    void testValidAndInvalidFflagsJsonAndForeignKeysPreserved() {
        SettingsManager mgr;
        const QString clientSettingsPath = mgr.clientAppSettingsPath();
        QDir().mkpath(QFileInfo(clientSettingsPath).absolutePath());
        {
            QFile f(clientSettingsPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(R"({"FFlagForeignMacOBloxKey": true})");
            f.close();
        }

        const QString cfgPath = mgr.soberBaseDir() + "/config/sober/config.json";
        QDir().mkpath(QFileInfo(cfgPath).absolutePath());
        {
            QFile f(cfgPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(R"({"custom_sober_internal_key": 42, "fflags": {"FFlagForeignUserKey": true}})");
            f.close();
        }

        mgr.settings().customFflagsJson = "{ broken json ";
        QString err;
        QVERIFY(!mgr.saveAll(&err));
        QVERIFY(!err.isEmpty());

        mgr.settings().customFflagsJson = R"({"FIntCameraMaxZoomDistance": 9999})";
        mgr.settings().targetFps = 360;
        mgr.settings().fullBright = true;
        mgr.settings().clearFog = true;
        QVERIFY(mgr.saveAll(&err));

        // 1. Verify MacOBlox ClientAppSettings.json
        QFile csf(clientSettingsPath);
        QVERIFY(csf.open(QIODevice::ReadOnly));
        QJsonObject csRoot = QJsonDocument::fromJson(csf.readAll()).object();
        QCOMPARE(csRoot.value("FFlagForeignMacOBloxKey").toBool(), true);
        QCOMPARE(csRoot.value("DFIntTaskSchedulerTargetFps").toInt(), 360);
        QCOMPARE(csRoot.value("FIntRenderShadowIntensity").toInt(), 0);
        QCOMPARE(csRoot.value("FIntFRMMinGrassDistance").toInt(), 0);
        QCOMPARE(csRoot.value("FFlagDebugGraphicsPreferOpenGL").toBool(), true);
        QVERIFY(!csRoot.contains("FFlagDebugGraphicsPreferVulkan"));

        // 2. Verify legacy config.json compatibility
        QFile cf(cfgPath);
        QVERIFY(cf.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(cf.readAll()).object();
        QCOMPARE(root.value("custom_sober_internal_key").toInt(), 42);
        QCOMPARE(root.value("use_console_experience").toBool(), false);
        QCOMPARE(root.value("touch_mode").toString(), QString("off"));
        QVERIFY(!root.contains("bring_back_oof"));
    }

    void testRegistryClampingMutualExclusionAndReset() {
        AppSettings s;
        QJsonObject badJson;
        badJson["targetFps"] = 999999;
        badJson["glassOpacityPct"] = -100;
        badJson["glassBlurStrength"] = 99;
        badJson["mouseSensitivityPct"] = 999;
        badJson["noShadows"] = true;
        badJson["ultraShadows"] = true;
        badJson["pipewireBuffer"] = true;
        badJson["audioSink"] = "alsa_output.pci";
        badJson["audioSource"] = "alsa_input.pci";

        SettingsRegistry::readFromJson(s, badJson);
        QCOMPARE(s.schemaVersion, 36);
        QCOMPARE(s.targetFps, 9999);
        QCOMPARE(s.glassOpacityPct, 2);
        QCOMPARE(s.glassBlurStrength, 8);
        QCOMPARE(s.mouseSensitivityPct, 200);
        QCOMPARE(s.noShadows, true);
        QCOMPARE(s.ultraShadows, false);

        QJsonObject written = SettingsRegistry::writeToJson(s);
        QCOMPARE(written.value("schemaVersion").toInt(), 36);
        QVERIFY(!written.contains("pipewireBuffer"));
        QVERIFY(!written.contains("audioSink"));
        QVERIFY(!written.contains("audioSource"));
        QVERIFY(!written.contains("bringBackOof"));
        QVERIFY(!written.contains("graphicsApi"));

        QVERIFY(SettingsRegistry::resetKey(s, "targetFps"));
        QCOMPARE(s.targetFps, 240);

        SettingsRegistry::applyVisualPreset(s, 4);
        QCOMPARE(s.animBg, false);
        QCOMPARE(s.reducedMotion, true);
        QCOMPARE(s.glassBlurStrength, 0);

        SettingsRegistry::resetVisualAppearance(s);
        QCOMPARE(s.animBg, true);
        QCOMPARE(s.glassBlurStrength, 4);
    }

    void testLegacySchemaMigration() {
        const QString cfgDir = m_tmpDir.path() + "/config";
        QDir().mkpath(cfgDir);
        const QString v36Path = cfgDir + "/settings_v36.json";
        const QString v35Path = cfgDir + "/settings_v35.json";
        QFile::remove(v36Path);

        const QByteArray v35Payload = R"({
            "schemaVersion": 35,
            "activeAccountId": "alt_slot",
            "accounts": [
                {"id": "default", "label": "Main", "displayName": "MainUser"},
                {"id": "alt_slot", "label": "Alt", "displayName": "AltUser"}
            ],
            "settings": {
                "targetFps": 360,
                "msaa": 3,
                "customMaxZoom": 99999,
                "glassBlurStrength": 6,
                "glassOpacityPct": 28,
                "lastPlaceInput": "2753915549",
                "gpuVendor": 1,
                "audioSink": "alsa_output.pci"
            }
        })";
        {
            QFile f(v35Path);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(v35Payload);
            f.close();
        }

        SettingsManager mgr;
        mgr.loadAll();

        QCOMPARE(mgr.settings().schemaVersion, 36);
        QCOMPARE(mgr.activeAccountId(), QString("alt_slot"));
        QCOMPARE(mgr.accounts().size(), 2);
        QCOMPARE(mgr.accounts().at(1).displayName, QString("AltUser"));
        QCOMPARE(mgr.settings().targetFps, 360);
        QCOMPARE(mgr.settings().msaa, MsaaMode::X4);
        QCOMPARE(mgr.settings().customMaxZoom, 9999);
        QCOMPARE(mgr.settings().glassBlurStrength, 6);
        QCOMPARE(mgr.settings().glassOpacityPct, 28);
        QCOMPARE(mgr.settings().lastPlaceInput, QString("2753915549"));
        QVERIFY2(QFileInfo::exists(v36Path), "Migrated settings_v36.json must be written automatically");
    }

    void testProfilesExportImportAndNamedProfiles() {
        SettingsManager mgr;
        mgr.settings().targetFps = 480;
        mgr.settings().mouseSensitivityPct = 135;
        mgr.settings().dnsProvider = 2;

        QString err;
        QVERIFY(mgr.saveNamedProfile("TestProfile240", &err));
        QVERIFY(mgr.listNamedProfiles().contains("TestProfile240"));

        SettingsRegistry::resetAll(mgr.settings());
        QCOMPARE(mgr.settings().targetFps, 240);

        QVERIFY(mgr.loadNamedProfile("TestProfile240", &err));
        QCOMPARE(mgr.settings().targetFps, 480);
        QCOMPARE(mgr.settings().mouseSensitivityPct, 135);
        QCOMPARE(mgr.settings().dnsProvider, 2);

        QVERIFY(mgr.deleteNamedProfile("TestProfile240"));
        QVERIFY(!mgr.listNamedProfiles().contains("TestProfile240"));
    }

    void testZeroAudioInterferenceAndSourceAudit() {
        SettingsManager mgr;
        SoberController ctrl(&mgr);
        const QStringList args = ctrl.buildFlatpakArgs("roblox://experiences/start?placeId=1818");
        const QString joinedArgs = args.join(' ');

        QVERIFY2(!joinedArgs.contains("PULSE", Qt::CaseInsensitive), "Found PULSE in launch args");
        QVERIFY2(!joinedArgs.contains("PIPEWIRE", Qt::CaseInsensitive), "Found PIPEWIRE in launch args");
        QVERIFY2(!joinedArgs.contains("ALSA", Qt::CaseInsensitive), "Found ALSA in launch args");
        QVERIFY2(!joinedArgs.contains("AUDIODRIVER", Qt::CaseInsensitive), "Found AUDIODRIVER in launch args");
        QVERIFY2(!joinedArgs.contains("pulseaudio", Qt::CaseInsensitive), "Found pulseaudio in launch args");
        QVERIFY2(joinedArgs.contains("libMacOBloxShims.dylib"), "Missing libMacOBloxShims.dylib in launch args");

        const QString srcDir = QFileInfo(__FILE__).absolutePath() + "/../src";
        QDirIterator it(srcDir, {"*.cpp", "*.hpp"}, QDir::Files, QDirIterator::Subdirectories);
        const QStringList forbiddenTokens = {
            "pactl", "wpctl", "PULSE_", "PIPEWIRE_", "ALSA_", "AUDIODRIVER",
            "runMicrophoneDiagnostics", "QWebEngineCookieStore", "SecretStore"
        };
        while (it.hasNext()) {
            QString filePath = it.next();
            QFile f(filePath);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QString content = QString::fromUtf8(f.readAll());
            for (const QString &tok : forbiddenTokens) {
                QVERIFY2(!content.contains(tok),
                         qPrintable(QString("Forbidden token '%1' found in %2").arg(tok, filePath)));
            }
        }
    }

    void testAudioHalMicrophoneFixesPresent() {
        const QString shimPath = QFileInfo(__FILE__).absolutePath() + "/../shims/audio_hal.c";
        QFile f(shimPath);
        QVERIFY2(f.open(QIODevice::ReadOnly), "shims/audio_hal.c must exist");
        const QString code = QString::fromUtf8(f.readAll());

        QVERIFY(code.contains("is_input_device"));
        QVERIFY(code.contains("kAudioOutputUnitProperty_SetInputCallback"));
        QVERIFY(code.contains("DYLD_INTERPOSE(t_unit_render, AudioUnitRender)"));
        QVERIFY(code.contains("unit_mic_thread"));
        QVERIFY(code.contains("MACOBLOX_MIC_FIFO"));
        QVERIFY(code.contains("MACOBLOX_AUDIO_LOG"));
    }

private:
    QTemporaryDir m_tmpDir;
};

QTEST_MAIN(TestMqBlox)
#include "tst_mqblox.moc"
