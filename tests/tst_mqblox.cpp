#include "core/SettingsManager.hpp"
#include <QtTest>

class TestMqBlox : public QObject {
    Q_OBJECT
private slots:
    void testParsePlaceUri() {
        QCOMPARE(mqblox::SettingsManager::parsePlaceUri("1818"),
                 QString("roblox://experiences/start?placeId=1818"));
        QCOMPARE(mqblox::SettingsManager::parsePlaceUri("https://www.roblox.com/games/2753915549/Blox-Fruits"),
                 QString("roblox://experiences/start?placeId=2753915549"));
    }
};

QTEST_MAIN(TestMqBlox)
#include "tst_mqblox.moc"
