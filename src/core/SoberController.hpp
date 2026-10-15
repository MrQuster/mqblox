#pragma once
#include "core/SettingsManager.hpp"
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVector>

namespace mqblox {

class SoberController : public QObject {
    Q_OBJECT
public:
    explicit SoberController(SettingsManager *settingsMgr, QObject *parent = nullptr);

    void detectHardwareAsync();
    void queryAudioDevicesAsync();

    void launchRoblox(const QString &customUri = {});
    void stopRobloxAsync();
    void healGameAudioStreamAsync();
    void switchAccountSlotAsync(const QString &targetAccountId);
    void saveCurrentSessionToSlotAsync();

    QString cpuModel() const { return m_cpuModel; }
    int cpuThreads() const { return m_cpuThreads; }
    QString gpuModel() const { return m_gpuModel; }
    QString detectedGpuVendor() const { return m_gpuVendor; }

    bool isGameRunning() const { return m_gameProc->state() != QProcess::NotRunning; }

signals:
    void hardwareDetected();
    void audioDevicesUpdated(const QVector<QPair<QString, QString>> &sinks,
                             const QVector<QPair<QString, QString>> &sources);
    void logGenerated(const QString &line);
    void gameStarted();
    void gameLeftByPlayer();
    void gameFinished(int exitCode, bool crashed);
    void accountSwitchCompleted(const QString &accountId);
    void toastRequested(const QString &message);

private:
    void handleProcessOutput(const QString &chunk);
    QStringList buildFlatpakArgs(const QString &targetUri) const;

    SettingsManager *m_settingsMgr;
    QProcess *m_gameProc;
    QString m_cpuModel = "CPU x86_64";
    int m_cpuThreads = 4;
    QString m_gpuModel = "GPU Linux";
    QString m_gpuVendor = "amd";

    bool m_manualStop = false;
    bool m_inActivePlace = false;
    qint64 m_lastTeleportMs = 0;
    qint64 m_joinTimestampMs = 0;
};

} // namespace mqblox
