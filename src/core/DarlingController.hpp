#pragma once
#include "core/SettingsManager.hpp"
#include <QHash>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <mutex>

class QNetworkAccessManager;
class QUdpSocket;

namespace mqblox {

struct EnvironmentStatus {
    bool hasDarling = false;
    bool hasClang = false;
    bool hasLld = false;
    bool hasUnzip = false;
    bool hasShimDylib = false;
    bool hasRobloxApp = false;
    QString installedRobloxVersion;
    QString gpuSummary;
    qint64 vramBytes = 4LL * 1024 * 1024 * 1024;
    QString sessionType;
};

class DarlingController : public QObject {
    Q_OBJECT
public:
    explicit DarlingController(SettingsManager *settingsMgr, QObject *parent = nullptr);
    ~DarlingController() override;

    void detectHardwareAsync();

    void launchRoblox(const QString &customUri = {});
    void stopRobloxAsync();
    void restartDarlingAsync();
    void switchAccountSlotAsync(const QString &targetAccountId);
    void saveCurrentSessionToSlotAsync();

    void runSetupOrUpdateWizardAsync();
    bool isWizardBusy() const { return m_wizardBusy; }

    EnvironmentStatus checkEnvironment() const;
    QString collectDiagnosticsReport() const;
    bool isSignedIn() const;
    void logoutAsync();

    QString shimSourceDir() const;
    QString shimDylibPath() const;

    QString cpuModel() const { return m_cpuModel; }
    int cpuThreads() const { return m_cpuThreads; }
    QString gpuModel() const { return m_gpuModel; }
    QString detectedGpuVendor() const { return m_gpuVendor; }
    qint64 detectedVramBytes() const { return m_vramBytes; }

    bool isGameRunning() const { return m_gameProc->state() != QProcess::NotRunning || isRobloxProcessAlive(); }
    static bool isRobloxProcessAlive();

    QStringList buildFlatpakArgs(const QString &targetUri = {}) const;
    QString buildDarlingLaunchScript(const QString &targetUri = {}) const;

    static QString darlingMacOBloxUserDir(const QString &darlingPrefix);
    static bool isTelemetryDomain(const QString &domain);
    static QByteArray buildNxDomainResponse(const QByteArray &query, int questionEnd);
    static QByteArray buildHostDnsResponse(const QByteArray &query);

signals:
    void hardwareDetected();
    void logGenerated(const QString &line);
    void gameStarted();
    void gameLeftByPlayer();
    void gameFinished(int exitCode, bool crashed);
    void accountSwitchCompleted(const QString &accountId);
    void toastRequested(const QString &message);
    void wizardProgress(int percent, const QString &statusText);
    void wizardFinished(bool ok, const QString &summary);
    void sessionStateChanged();

private:
    void handleProcessOutput(const QString &chunk);
    void startDnsForwarder();
    void stopDnsForwarder();
    void clearStaleDarlingProcesses();
    void prepareDarlingPrefixSync();
    void stepDownloadOrUpdateRoblox();
    bool hasMultipleGpus() const;

    SettingsManager *m_settingsMgr = nullptr;
    QProcess *m_gameProc = nullptr;
    QProcess *m_wizardProc = nullptr;
    QTimer *m_launchWatchdog = nullptr;
    QPointer<QTimer> m_procMonitorTimer;
    QNetworkAccessManager *m_nam = nullptr;
    QUdpSocket *m_dnsUdp = nullptr;
    quint16 m_dnsPort = 0;
    QString m_dnsUpstreamIp;
    mutable std::mutex m_dnsCacheMutex;
    QHash<QString, QByteArray> m_dnsCache;
    bool m_wizardBusy = false;

    QString m_cpuModel = "CPU x86_64";
    int m_cpuThreads = 4;
    QString m_gpuModel = "GPU Linux";
    QString m_gpuVendor = "amd";
    qint64 m_vramBytes = 4LL * 1024 * 1024 * 1024;

    bool m_manualStop = false;
    bool m_inActivePlace = false;
    qint64 m_lastTeleportMs = 0;
    qint64 m_joinTimestampMs = 0;
    QStringList m_recentErrors;
};

} // namespace mqblox
