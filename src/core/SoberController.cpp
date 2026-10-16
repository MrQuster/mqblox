#include "core/SoberController.hpp"
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QThread>
#include <QtConcurrent>

namespace mqblox {

static void copyDirectoryContent(const QString &srcPath, const QString &dstPath) {
    QDir src(srcPath);
    if (!src.exists()) return;
    QDir dst(dstPath);
    if (!dst.exists()) dst.mkpath(".");
    for (const QFileInfo &info : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString sItem = info.filePath();
        QString dItem = dst.filePath(info.fileName());
        if (info.isDir()) copyDirectoryContent(sItem, dItem);
        else { QFile::remove(dItem); QFile::copy(sItem, dItem); }
    }
}

SoberController::SoberController(SettingsManager *settingsMgr, QObject *parent)
    : QObject(parent),
      m_settingsMgr(settingsMgr),
      m_gameProc(new QProcess(this)) {
    m_cpuThreads = std::max(2, QThread::idealThreadCount());

    connect(m_gameProc, &QProcess::readyReadStandardOutput, this, [this]() {
        handleProcessOutput(QString::fromUtf8(m_gameProc->readAllStandardOutput()));
    });
    connect(m_gameProc, &QProcess::readyReadStandardError, this, [this]() {
        handleProcessOutput(QString::fromUtf8(m_gameProc->readAllStandardError()));
    });
    connect(m_gameProc, &QProcess::started, this, [this]() {
        emit gameStarted();
    });
    connect(m_gameProc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        m_inActivePlace = false;
        bool crashed = (!m_manualStop && (status == QProcess::CrashExit || exitCode != 0));
        emit gameFinished(exitCode, crashed);
    });
}

void SoberController::healGameAudioStreamAsync() {
    QProcess::startDetached("pactl", {"unload-module", "module-role-cork"});
    QProcess::startDetached("pactl", {"set-sink-mute", "@DEFAULT_SINK@", "0"});
}

void SoberController::detectHardwareAsync() {
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly)) {
        for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
            if (line.startsWith("model name")) {
                m_cpuModel = line.section(':', 1).trimmed();
                break;
            }
        }
    }

    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc](int, QProcess::ExitStatus) {
        for (const QString &line : QString::fromUtf8(proc->readAllStandardOutput()).split('\n')) {
            if (line.contains("VGA") || line.contains("3D controller") || line.contains("Display controller")) {
                m_gpuModel = line.section(':', 2).trimmed();
                QString low = m_gpuModel.toLower();
                if (low.contains("nvidia") || low.contains("geforce")) m_gpuVendor = "nvidia";
                else if (low.contains("amd") || low.contains("radeon") || low.contains("ati")) m_gpuVendor = "amd";
                else if (low.contains("intel")) m_gpuVendor = "intel";
                break;
            }
        }
        proc->deleteLater();
        emit hardwareDetected();
    });
    proc->start("lspci", {});
}

void SoberController::queryAudioDevicesAsync() {
    // Sober сам управляет выбором микрофона и наушников в меню игры (Esc -> Settings)
}

QStringList SoberController::buildFlatpakArgs(const QString &targetUri) const {
    const AppSettings &s = m_settingsMgr->settings();
    QStringList args = {
        "run",
        "--socket=wayland",
        "--socket=fallback-x11",
        "--socket=x11",
        "--socket=pulseaudio",
        "--device=dri",
        "--filesystem=xdg-run/pipewire-0",
        "--talk-name=org.freedesktop.portal.Desktop",
        "--talk-name=org.freedesktop.portal.GameMode"
    };

    // Никакого принудительного перехвата PULSE_SOURCE / PULSE_SINK: Sober выбирает микрофон сам!
    if (s.pipewireBuffer) {
        args << "--env=PIPEWIRE_LATENCY=1024/48000";
    }
    if (s.mallocTrim) {
        args << "--env=MALLOC_TRIM_THRESHOLD_=65536" << "--env=MALLOC_ARENA_MAX=2";
    }

    if (s.windowDriver == WindowDriverMode::ForceX11) args << "--env=SDL_VIDEODRIVER=x11";
    else if (s.windowDriver == WindowDriverMode::ForceWayland) args << "--env=SDL_VIDEODRIVER=wayland";

    QString effGpu = m_gpuVendor;
    if (s.gpuVendor == GpuVendorMode::AMD) effGpu = "amd";
    else if (s.gpuVendor == GpuVendorMode::Nvidia) effGpu = "nvidia";
    else if (s.gpuVendor == GpuVendorMode::Intel) effGpu = "intel";

    if (effGpu == "nvidia") {
        if (s.glThread) args << "--env=__GL_THREADED_OPTIMIZATIONS=1";
        args << "--env=__GL_SHADER_DISK_CACHE=1"
             << "--env=__GL_SHADER_DISK_CACHE_SKIP_CLEANUP=1"
             << "--env=__GL_YIELD=USLEEP";
    } else if (effGpu == "intel") {
        if (s.glThread) args << "--env=mesa_glthread=true";
        args << "--env=ANV_GPL=true";
    } else {
        if (s.glThread) args << "--env=mesa_glthread=true";
        if (s.asyncShaders) args << "--env=radeonsi_sync_compile=false" << "--env=RADV_PERFTEST=gpl,sam";
        args << "--env=MESA_SHADER_CACHE_MAX_SIZE=4G";
    }

    if (s.showHudFps) {
        args << "--env=GALLIUM_HUD=fps" << "--env=MANGOHUD=1";
    }
    if (s.vsyncOff || s.unlimitedFps) {
        args << "--env=vblank_mode=0"
             << "--env=__GL_SYNC_TO_VBLANK=0"
             << "--env=MESA_VK_WSI_PRESENT_MODE=immediate"
             << "--env=SDL_RENDER_VSYNC=0";
    }

    args << "org.vinegarhq.Sober";
    if (!targetUri.isEmpty()) args << targetUri;
    return args;
}

void SoberController::launchRoblox(const QString &customUri) {
    m_manualStop = false;
    m_inActivePlace = false;
    m_settingsMgr->saveAll();

    if (m_settingsMgr->settings().smartAutoCleanCache) {
        QString httpCache = m_settingsMgr->soberBaseDir() + "/cache/sober/http";
        (void)QtConcurrent::run([httpCache]() {
            QDir d(httpCache);
            if (!d.exists()) return;
            qint64 bytes = 0;
            QDirIterator it(httpCache, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) { it.next(); bytes += it.fileInfo().size(); }
            if (bytes > 500LL * 1024 * 1024) {
                QDir(httpCache).removeRecursively();
                QDir().mkpath(httpCache);
            }
        });
    }

    QString targetUri = !customUri.isEmpty()
                            ? customUri
                            : SettingsManager::parsePlaceUri(m_settingsMgr->settings().lastPlaceInput);

    auto startClient = [this, targetUri]() {
        QStringList args = buildFlatpakArgs(targetUri);
        emit logGenerated("[🚀] Запуск Sober: flatpak " + args.join(' '));
        m_gameProc->start("flatpak", args);
    };

    // ВСЕГДА завершаем старый фоновый процесс Sober перед стартом, чтобы он гарантированно перечитал новый config.json!
    auto *killProc = new QProcess(this);
    connect(killProc, &QProcess::finished, this, [killProc, startClient](int, QProcess::ExitStatus) {
        killProc->deleteLater();
        QTimer::singleShot(180, startClient);
    });
    killProc->start("flatpak", {"kill", "org.vinegarhq.Sober"});
}

void SoberController::stopRobloxAsync() {
    m_manualStop = true;
    m_inActivePlace = false;
    auto *killProc = new QProcess(this);
    connect(killProc, &QProcess::finished, killProc, &QObject::deleteLater);
    killProc->start("flatpak", {"kill", "org.vinegarhq.Sober"});
    if (isGameRunning()) {
        m_gameProc->terminate();
    }
}

void SoberController::saveCurrentSessionToSlotAsync() {
    QString appData = m_settingsMgr->soberBaseDir() + "/data/sober/appData";
    QString slotDir = m_settingsMgr->accountsDir() + "/" + m_settingsMgr->activeAccountId() + "/appData";
    (void)QtConcurrent::run([appData, slotDir]() {
        if (QDir(appData).exists()) {
            QDir(slotDir).removeRecursively();
            copyDirectoryContent(appData, slotDir);
        }
    });
}

void SoberController::switchAccountSlotAsync(const QString &targetAccountId) {
    stopRobloxAsync();
    QString appData = m_settingsMgr->soberBaseDir() + "/data/sober/appData";
    QString oldSlot = m_settingsMgr->accountsDir() + "/" + m_settingsMgr->activeAccountId() + "/appData";
    QString newSlot = m_settingsMgr->accountsDir() + "/" + targetAccountId + "/appData";

    m_settingsMgr->setActiveAccountId(targetAccountId);
    m_settingsMgr->saveAll();

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, targetAccountId]() {
        watcher->deleteLater();
        emit accountSwitchCompleted(targetAccountId);
    });
    watcher->setFuture(QtConcurrent::run([appData, oldSlot, newSlot]() {
        if (QDir(appData).exists()) {
            QDir(oldSlot).removeRecursively();
            copyDirectoryContent(appData, oldSlot);
        }
        QDir(appData).removeRecursively();
        if (QDir(newSlot).exists()) {
            copyDirectoryContent(newSlot, appData);
        } else {
            QDir().mkpath(appData);
        }
    }));
}

void SoberController::handleProcessOutput(const QString &chunk) {
    const QStringList &markers = m_settingsMgr->settings().leaveMarkers;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const QString &rawLine : chunk.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.contains("ROBLOSECURITY", Qt::CaseInsensitive) || line.contains("Bearer ", Qt::CaseInsensitive)) {
            continue;
        }
        emit logGenerated(line);

        if (line.contains("Joining game") || line.contains("Connecting to UDMUX") || line.contains("Connection accepted")) {
            m_inActivePlace = true;
            m_joinTimestampMs = now;
        }
        if (line.contains("Teleport", Qt::CaseInsensitive)) {
            m_lastTeleportMs = now;
        }

        if (m_inActivePlace && m_settingsMgr->settings().returnToMqBloxOnLeave) {
            if ((now - m_joinTimestampMs > 4000) && (now - m_lastTeleportMs > 4000)) {
                for (const QString &pat : markers) {
                    if (!pat.isEmpty() && line.contains(pat)) {
                        m_inActivePlace = false;
                        stopRobloxAsync();
                        emit gameLeftByPlayer();
                        break;
                    }
                }
            }
        }
    }
}

} // namespace mqblox
