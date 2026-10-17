#include "core/SettingsManager.hpp"
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

    void testValidAndInvalidFflagsJson() {
        SettingsManager mgr;
        mgr.settings().customFflagsJson = "{ broken json ";
        QString err;
        QVERIFY(!mgr.saveAll(&err));
        QVERIFY(!err.isEmpty());

        mgr.settings().customFflagsJson = R"({"FIntCameraMaxZoomDistance": 9999})";
        mgr.settings().targetFps = 360;
        mgr.settings().fullBright = true;
        mgr.settings().clearFog = true;
        mgr.settings().glassOpacityPct = 14;
        QVERIFY(mgr.saveAll(&err));

        QFile cf(mgr.soberBaseDir() + "/config/sober/config.json");
        QVERIFY(cf.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(cf.readAll()).object();
        QJsonObject ff = root.value("fflags").toObject();
        QCOMPARE(ff.value("DFIntTaskSchedulerTargetFps").toInt(), 360);
        QCOMPARE(ff.value("FIntRenderShadowIntensity").toInt(), 0);
        QCOMPARE(ff.value("FIntFRMMinGrassDistance").toInt(), 0);
    }
};

QTEST_MAIN(TestMqBlox)
#include "tst_mqblox.moc"
