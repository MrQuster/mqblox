#include "core/SettingsManager.hpp"
#include "core/SettingsRegistry.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace mqblox;

class TestMqBlox : public QObject {
    Q_OBJECT
private slots:
    void testPlaceUriParser() {
        QCOMPARE(SettingsManager::parsePlaceUri("1818"),
                 QString("roblox://experiences/start?placeId=1818"));
        QCOMPARE(SettingsManager::parsePlaceUri("https://www.roblox.com/games/2753915549/Blox-Fruits?privateServerLinkCode=ABC123xyz"),
                 QString("roblox://experiences/start?placeId=2753915549&linkCode=ABC123xyz"));
        QCOMPARE(SettingsManager::parsePlaceUri("invalid_text"), QString());
    }

    void testValidAndInvalidFflagsJsonAndForeignKeysPreserved() {
        SettingsManager mgr;
        QString cfgPath = mgr.soberBaseDir() + "/config/sober/config.json";

        // Записываем сторонний ключ в config.json Sober и проверяем, что лаунчер его НЕ затирает
        {
            QFile f(cfgPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(R"({"custom_sober_internal_key": 42, "fflags": {"FFlagForeignUserKey": true}})");
                f.close();
            }
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

        QFile cf(cfgPath);
        QVERIFY(cf.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(cf.readAll()).object();
        QCOMPARE(root.value("custom_sober_internal_key").toInt(), 42);
        QCOMPARE(root.value("use_console_experience").toBool(), false);
        QCOMPARE(root.value("touch_mode").toString(), QString("off"));

        QJsonObject ff = root.value("fflags").toObject();
        QCOMPARE(ff.value("FFlagForeignUserKey").toBool(), true);
        QCOMPARE(ff.value("DFIntTaskSchedulerTargetFps").toInt(), 360);
        QCOMPARE(ff.value("FIntRenderShadowIntensity").toInt(), 0);
        QCOMPARE(ff.value("FIntFRMMinGrassDistance").toInt(), 0);
    }

    void testRegistryClampingMutualExclusionAndReset() {
        AppSettings s;
        QJsonObject badJson;
        badJson["targetFps"] = 999999;
        badJson["glassOpacityPct"] = -100;
        badJson["glassBlurStrength"] = 99;
        badJson["noclipCamWalls"] = true;
        badJson["noShadows"] = true;
        badJson["ultraShadows"] = true; // Конфликт с noShadows — должен сброситься в false

        SettingsRegistry::readFromJson(s, badJson);
        QCOMPARE(s.schemaVersion, 38);
        QCOMPARE(s.targetFps, 9999);
        QCOMPARE(s.glassOpacityPct, 2);
        QCOMPARE(s.glassBlurStrength, 8);
        QCOMPARE(s.noclipCamWalls, false);
        QCOMPARE(s.noShadows, true);
        QCOMPARE(s.ultraShadows, false);

        QVERIFY(SettingsRegistry::resetKey(s, "targetFps"));
        QCOMPARE(s.targetFps, 240);

        SettingsRegistry::applyVisualPreset(s, 4); // Экономный пресет
        QCOMPARE(s.animBg, false);
        QCOMPARE(s.reducedMotion, true);
        QCOMPARE(s.glassBlurStrength, 0);

        SettingsRegistry::resetVisualAppearance(s);
        QCOMPARE(s.animBg, true);
        QCOMPARE(s.glassBlurStrength, 4);
    }
};

QTEST_MAIN(TestMqBlox)
#include "tst_mqblox.moc"
