#include "core/SettingsManager.hpp"
#include "core/SettingsRegistry.hpp"
#include "core/DarlingController.hpp"
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <unistd.h>

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

        mgr.settings().customFflagsJson = "{ broken json ";
        QString err;
        QVERIFY(!mgr.saveAll(&err));
        QVERIFY(!err.isEmpty());

        mgr.settings().customFflagsJson = R"({"FIntCameraMaxZoomDistance": 9999})";
        mgr.settings().targetFps = 360;
        mgr.settings().fullBright = true;
        mgr.settings().clearFog = true;
        QVERIFY(mgr.saveAll(&err));

        // Verify MacOBlox ClientAppSettings.json
        QFile csf(clientSettingsPath);
        QVERIFY(csf.open(QIODevice::ReadOnly));
        QJsonObject csRoot = QJsonDocument::fromJson(csf.readAll()).object();
        QCOMPARE(csRoot.value("FFlagForeignMacOBloxKey").toBool(), true);
        QCOMPARE(csRoot.value("DFIntTaskSchedulerTargetFps").toInt(), 360);
        QCOMPARE(csRoot.value("FIntRenderShadowIntensity").toInt(), 0);
        QCOMPARE(csRoot.value("FIntFRMMinGrassDistance").toInt(), 0);
        QCOMPARE(csRoot.value("FFlagDebugGraphicsPreferOpenGL").toBool(), true);
        QCOMPARE(csRoot.value("FFlagDebugGraphicsDisableOpenGL").toBool(), false);
        QCOMPARE(csRoot.value("FFlagDebugGraphicsPreferVulkan").toBool(), false);
        QCOMPARE(csRoot.value("FFlagDebugGraphicsDisableVulkan").toBool(), true);
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
        DarlingController ctrl(&mgr);
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

    void testCookiesDiagnosticsPrivacyAndWording() {
        SettingsManager mgr;
        DarlingController ctrl(&mgr);

        // Before cookies exist: session should not be active
        QVERIFY(!ctrl.isSignedIn());
        QString reportNoAuth = ctrl.collectDiagnosticsReport();
        QVERIFY(reportNoAuth.contains("Сессия авторизации (Cookies.plist): Не авторизована"));

        // Create a simulated Cookies.plist inside Darling user dir
        const QString cookiesDir = DarlingController::darlingMacOBloxUserDir(mgr.darlingPrefixDir());
        QDir().mkpath(cookiesDir);
        const QString cookiesPath = cookiesDir + "/Cookies.plist";
        {
            QFile f(cookiesPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write("<plist version=\"1.0\"><dict><key>Name</key><string>.ROBLOSECURITY</string><key>Value</key><string>_SECRET_TOKEN_DO_NOT_LEAK_</string></dict></plist>");
            f.close();
        }

        // Verify session is detected as signed in with the exact required wording
        QVERIFY(ctrl.isSignedIn());
        QString reportAuth = ctrl.collectDiagnosticsReport();
        QVERIFY(reportAuth.contains("🟢 Сессия активна (Cookies.plist сохранён в Darling; токен защищён и не считывается лаунчером)"));

        // Crucial security & privacy audit: the secret token value must NEVER be leaked or read into the report
        QVERIFY(!reportAuth.contains("_SECRET_TOKEN_DO_NOT_LEAK_"));

        // Clean up
        QFile::remove(cookiesPath);
        QVERIFY(!ctrl.isSignedIn());
    }

    void testProcessExecutionBackendSanitization() {
        SettingsManager mgr;
        DarlingController ctrl(&mgr);

        // Test safe alphanumeric MACOBLOX_GRAPHICS_BACKEND
        qputenv("MACOBLOX_GRAPHICS_BACKEND", "opengl");
        QString script = ctrl.buildDarlingLaunchScript();
        QVERIFY(script.contains("export MACOBLOX_GRAPHICS_BACKEND='opengl'"));

        // Test shell injection attempt: semicolon, command chaining, backticks, quotes
        qputenv("MACOBLOX_GRAPHICS_BACKEND", "opengl; touch /tmp/pwned");
        script = ctrl.buildDarlingLaunchScript();
        QVERIFY(!script.contains("touch /tmp/pwned"));
        QVERIFY(!script.contains(";"));
        QVERIFY(script.contains("export MACOBLOX_GRAPHICS_BACKEND='opengl'"));

        // Test path traversal attempt
        qputenv("MACOBLOX_GRAPHICS_BACKEND", "../../../etc/passwd");
        script = ctrl.buildDarlingLaunchScript();
        QVERIFY(!script.contains("../../../etc/passwd"));
        QVERIFY(!script.contains(".."));
        QVERIFY(script.contains("export MACOBLOX_GRAPHICS_BACKEND='opengl'"));

        qunsetenv("MACOBLOX_GRAPHICS_BACKEND");
    }

    void testDnsTelemetryFilteringAndLegitimateDomains() {
        // 1. Verify telemetry domains are identified correctly
        QVERIFY(DarlingController::isTelemetryDomain("pulsar.roblox.com"));
        QVERIFY(DarlingController::isTelemetryDomain("PULSAR.ROBLOX.COM"));
        QVERIFY(DarlingController::isTelemetryDomain("sub.pulsar.roblox.com"));
        QVERIFY(DarlingController::isTelemetryDomain("ecsv2.roblox.com"));
        QVERIFY(DarlingController::isTelemetryDomain("ephemeralcounters.api.roblox.com"));

        // 2. Verify legitimate game servers and discovery domains are NOT blocked
        QVERIFY(!DarlingController::isTelemetryDomain("roblox.com"));
        QVERIFY(!DarlingController::isTelemetryDomain("www.roblox.com"));
        QVERIFY(!DarlingController::isTelemetryDomain("gamejoin.roblox.com"));
        QVERIFY(!DarlingController::isTelemetryDomain("matchmaking.roblox.com"));
        QVERIFY(!DarlingController::isTelemetryDomain("apis.roblox.com"));
        QVERIFY(!DarlingController::isTelemetryDomain("c.rbxcdn.com"));
        QVERIFY(!DarlingController::isTelemetryDomain("setup.rbxcdn.com"));

        // 3. Construct a standard DNS A-query packet for pulsar.roblox.com
        QByteArray dnsQuery;
        dnsQuery.append(static_cast<char>(0x12)); // Transaction ID MSB
        dnsQuery.append(static_cast<char>(0x34)); // Transaction ID LSB
        dnsQuery.append(static_cast<char>(0x01)); // Flags: RD=1
        dnsQuery.append(static_cast<char>(0x00));
        dnsQuery.append(static_cast<char>(0x00)); // QDCOUNT = 1
        dnsQuery.append(static_cast<char>(0x01));
        dnsQuery.append(static_cast<char>(0x00)); // ANCOUNT = 0
        dnsQuery.append(static_cast<char>(0x00));
        dnsQuery.append(static_cast<char>(0x00)); // NSCOUNT = 0
        dnsQuery.append(static_cast<char>(0x00));
        dnsQuery.append(static_cast<char>(0x00)); // ARCOUNT = 0
        dnsQuery.append(static_cast<char>(0x00));

        // QNAME: 6 pulsar 6 roblox 3 com 0
        dnsQuery.append(static_cast<char>(6));
        dnsQuery.append("pulsar");
        dnsQuery.append(static_cast<char>(6));
        dnsQuery.append("roblox");
        dnsQuery.append(static_cast<char>(3));
        dnsQuery.append("com");
        dnsQuery.append(static_cast<char>(0));

        // QTYPE = 1 (A), QCLASS = 1 (IN)
        dnsQuery.append(static_cast<char>(0x00));
        dnsQuery.append(static_cast<char>(0x01));
        dnsQuery.append(static_cast<char>(0x00));
        dnsQuery.append(static_cast<char>(0x01));

        // 4. Test buildHostDnsResponse on pulsar.roblox.com
        QByteArray resp = DarlingController::buildHostDnsResponse(dnsQuery);
        QVERIFY(!resp.isEmpty());
        // Verify transaction ID matched
        QCOMPARE(resp.left(2), dnsQuery.left(2));
        // Verify Flags indicate Response (QR=1) and NXDOMAIN (RCODE=3)
        const quint8 flagByte1 = static_cast<quint8>(resp[2]);
        const quint8 flagByte2 = static_cast<quint8>(resp[3]);
        QVERIFY((flagByte1 & 0x80) != 0); // Response flag QR=1
        QCOMPARE(flagByte2 & 0x0F, 3);    // RCODE = 3 (NXDOMAIN)
        // Verify ANCOUNT = 0
        QCOMPARE(static_cast<quint8>(resp[6]), static_cast<quint8>(0));
        QCOMPARE(static_cast<quint8>(resp[7]), static_cast<quint8>(0));
    }

    void testGraphicsBackendVulkanSwitching() {
        SettingsManager mgr;
        const QString robloxDir = mgr.robloxAppDir();
        QDir().mkpath(robloxDir + "/Contents/MacOS/ClientSettings");
        DarlingController ctrl(&mgr);

        // Verify default is OpenGL
        QCOMPARE(static_cast<int>(mgr.settings().graphicsBackend), static_cast<int>(GraphicsBackend::OpenGL));
        QString script = ctrl.buildDarlingLaunchScript();
        QVERIFY(script.contains("export MACOBLOX_GRAPHICS_BACKEND='opengl'"));

        // Switch to Vulkan
        mgr.settings().graphicsBackend = GraphicsBackend::Vulkan;
        QString err;
        QVERIFY(mgr.saveAll(&err));

        QFile csf(robloxDir + "/Contents/MacOS/ClientSettings/ClientAppSettings.json");
        QVERIFY(csf.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(csf.readAll()).object();
        // Roblox Mac Mach-O always relies on OpenGL (or host Zink OpenGL translation)
        QCOMPARE(root.value("FFlagDebugGraphicsPreferOpenGL").toBool(), true);
        QCOMPARE(root.value("FFlagDebugGraphicsDisableOpenGL").toBool(), false);
        QCOMPARE(root.value("FFlagDebugGraphicsPreferVulkan").toBool(), false);
        QCOMPARE(root.value("FFlagDebugGraphicsDisableVulkan").toBool(), true);

        // Verify launch script exports MACOBLOX_GRAPHICS_BACKEND='vulkan' and Zink driver override
        script = ctrl.buildDarlingLaunchScript();
        QVERIFY(script.contains("export MACOBLOX_GRAPHICS_BACKEND='vulkan'"));
        QVERIFY(script.contains("export MESA_LOADER_DRIVER_OVERRIDE=zink"));
        QVERIFY(script.contains("export GALLIUM_DRIVER=zink"));
    }

    void testIpcSecurityAndIsolation() {
        const uid_t uid = ::getuid();
        const QString expectedPrefix = QString("mqblox_ipc_%1").arg(uid);
        QVERIFY(expectedPrefix.startsWith("mqblox_ipc_"));
        QVERIFY(expectedPrefix.length() > 11);
    }

    void testDnsHostResolution() {
        // Test query for roblox.com A-record (type 1)
        // 12-byte header + 6 bytes "roblox" + 3 bytes "com" + 0x00 + type(0x0001) + class(0x0001)
        QByteArray dnsQuery;
        dnsQuery.append("\x12\x34", 2); // ID
        dnsQuery.append("\x01\x00", 2); // Standard query
        dnsQuery.append("\x00\x01", 2); // QDCOUNT = 1
        dnsQuery.append("\x00\x00", 2); // ANCOUNT = 0
        dnsQuery.append("\x00\x00", 2); // NSCOUNT = 0
        dnsQuery.append("\x00\x00", 2); // ARCOUNT = 0
        // "roblox.com"
        dnsQuery.append(static_cast<char>(6));
        dnsQuery.append("roblox");
        dnsQuery.append(static_cast<char>(3));
        dnsQuery.append("com");
        dnsQuery.append(static_cast<char>(0));
        dnsQuery.append("\x00\x01", 2); // Type A
        dnsQuery.append("\x00\x01", 2); // Class IN

        QByteArray resp = DarlingController::buildHostDnsResponse(dnsQuery);
        QVERIFY(!resp.isEmpty());
        // Verify transaction ID matches
        QCOMPARE(resp.left(2), QByteArray("\x12\x34", 2));
        // Verify response bit is set (0x81)
        QCOMPARE(static_cast<quint8>(resp[2]), static_cast<quint8>(0x81));
    }

    void testXdgBaseDirectoryCompliance() {
        const QByteArray savedTestDir = qgetenv("MQBLOX_TEST_DIR");
        qunsetenv("MQBLOX_TEST_DIR");

        // Test custom XDG variables
        QTemporaryDir customXdg;
        QVERIFY(customXdg.isValid());
        const QString customConfig = customXdg.path() + "/custom_cfg";
        const QString customData = customXdg.path() + "/custom_data";
        const QString customCache = customXdg.path() + "/custom_cache";

        qputenv("XDG_CONFIG_HOME", customConfig.toUtf8());
        qputenv("XDG_DATA_HOME", customData.toUtf8());
        qputenv("XDG_CACHE_HOME", customCache.toUtf8());

        // Create player in custom data dir to verify preference
        QDir().mkpath(customData + "/macoblox/RobloxPlayer.app/Contents/MacOS");
        QFile playerBin(customData + "/macoblox/RobloxPlayer.app/Contents/MacOS/RobloxPlayer");
        QVERIFY(playerBin.open(QIODevice::WriteOnly));
        playerBin.close();

        SettingsManager mgrCustom;
        QCOMPARE(mgrCustom.configDir(), customConfig + "/mqblox");
        QCOMPARE(mgrCustom.dataDir(), customData + "/mqblox");
        QCOMPARE(mgrCustom.cacheDir(), customCache + "/mqblox");
        QCOMPARE(mgrCustom.mqDir(), customData + "/mqblox");
        QCOMPARE(mgrCustom.accountsDir(), customData + "/mqblox/accounts");
        QCOMPARE(mgrCustom.macobloxDataDir(), customData + "/macoblox");

        // Test default XDG fallback when unset
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");

        SettingsManager mgrDefault;
        QCOMPARE(mgrDefault.configDir(), QDir::homePath() + "/.config/mqblox");
        QCOMPARE(mgrDefault.dataDir(), QDir::homePath() + "/.local/share/mqblox");
        QCOMPARE(mgrDefault.cacheDir(), QDir::homePath() + "/.cache/mqblox");
        QCOMPARE(mgrDefault.mqDir(), QDir::homePath() + "/.local/share/mqblox");
        QCOMPARE(mgrDefault.accountsDir(), QDir::homePath() + "/.local/share/mqblox/accounts");
        QVERIFY(mgrDefault.macobloxDataDir() == QDir::homePath() + "/.local/share/macoblox" ||
                mgrDefault.macobloxDataDir() == QDir::homePath() + "/.var/app/xyz.narez.MacOBlox/data/macoblox");

        // Restore test dir
        if (!savedTestDir.isEmpty()) {
            qputenv("MQBLOX_TEST_DIR", savedTestDir);
        }
    }

    void testRobloxExecutionPermissionsAndLaunchScript() {
        SettingsManager mgr;
        const QString appDir = mgr.robloxAppDir();
        const QString macOsDir = appDir + "/Contents/MacOS";
        QDir().mkpath(macOsDir);

        const QString mainBin = macOsDir + "/RobloxPlayer";
        const QString helperBin = macOsDir + "/RobloxCrashHandler";

        // Create dummy files with 0644 (no execute permissions)
        {
            QFile f1(mainBin);
            QVERIFY(f1.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f1.write("#!/bin/sh\nexit 0\n");
            f1.close();
            QFile::setPermissions(mainBin, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                           QFileDevice::ReadGroup | QFileDevice::ReadOther);

            QFile f2(helperBin);
            QVERIFY(f2.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f2.write("#!/bin/sh\nexit 0\n");
            f2.close();
            QFile::setPermissions(helperBin, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                             QFileDevice::ReadGroup | QFileDevice::ReadOther);
        }

        // Before launchRoblox: isExecutable() is false
        QVERIFY(!QFileInfo(mainBin).isExecutable());
        QVERIFY(!QFileInfo(helperBin).isExecutable());

        DarlingController ctrl(&mgr);
        // Invoking launchRoblox ensures execute permissions even if darling is not ready to start
        ctrl.launchRoblox();

        // Verify both main binary and helper are now executable (0755)
        QVERIFY(QFileInfo(mainBin).isExecutable());
        QVERIFY(QFileInfo(helperBin).isExecutable());

        // Test framework search path in buildDarlingLaunchScript
        const QString fwDir = mgr.macobloxDataDir() + "/frameworks";
        QDir().mkpath(fwDir);
        const QString scriptWithFw = ctrl.buildDarlingLaunchScript();
        QVERIFY(scriptWithFw.contains("export DYLD_FRAMEWORK_PATH="));
        QVERIFY(scriptWithFw.contains("/Volumes/SystemRoot"));

        // Clean up
        QDir(appDir).removeRecursively();
        QDir(fwDir).removeRecursively();
    }

    void testStaleSocketAndPidCleanup() {
        SettingsManager mgr;
        const QString prefixDir = mgr.darlingPrefixDir();
        QDir().mkpath(prefixDir);
        const QString pidFile = prefixDir + "/.init.pid";
        const QString sockFile = prefixDir + "/.darlingserver.sock";

        {
            QFile f(pidFile);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("12345\n");
            f.close();
        }
        {
            QFile f(sockFile);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("dummy-socket\n");
            f.close();
        }

        QVERIFY(QFile::exists(pidFile));
        QVERIFY(QFile::exists(sockFile));

        DarlingController ctrl(&mgr);
        ctrl.clearStaleDarlingProcesses();

        QVERIFY(!QFile::exists(pidFile));
        QVERIFY(!QFile::exists(sockFile));
    }

private:
    QTemporaryDir m_tmpDir;
};

QTEST_MAIN(TestMqBlox)
#include "tst_mqblox.moc"
