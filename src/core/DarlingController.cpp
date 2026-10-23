#include "core/DarlingController.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QtConcurrent>
#include <algorithm>
#include <csignal>
#include <unistd.h>

namespace mqblox {

namespace {

void copyDirectoryContent(const QString &srcPath, const QString &dstPath) {
    QDir src(srcPath);
    if (!src.exists()) return;
    QDir dst(dstPath);
    if (!dst.exists()) dst.mkpath(".");
    for (const QFileInfo &info : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString sItem = info.filePath();
        QString dItem = dst.filePath(info.fileName());
        if (info.isDir()) {
            copyDirectoryContent(sItem, dItem);
        } else {
            QFile::remove(dItem);
            QFile::copy(sItem, dItem);
        }
    }
}

qint64 readSysfsVramBytes() {
    qint64 bestVram = 0;
    QDir drmDir("/sys/class/drm");
    const QStringList cards = drmDir.entryList({"card*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &card : cards) {
        if (card.contains('-')) continue;
        QFile vramFile(drmDir.filePath(card + "/device/mem_info_vram_total"));
        if (vramFile.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const qint64 val = QString::fromLatin1(vramFile.readAll()).trimmed().toLongLong(&ok);
            if (ok && val > bestVram) {
                bestVram = val;
            }
        }
    }
    return bestVram > 0 ? bestVram : (4LL * 1024 * 1024 * 1024);
}

QString readInstalledRobloxVersion(const QString &robloxAppDir) {
    QFile plist(robloxAppDir + "/Contents/Info.plist");
    if (!plist.open(QIODevice::ReadOnly)) return {};
    const QString text = QString::fromUtf8(plist.readAll());
    static const QRegularExpression re(
        R"(<key>CFBundleShortVersionString</key>\s*<string>([^<]+)</string>)");
    const QRegularExpressionMatch m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString shellSingleQuote(const QString &s) {
    QString out = s;
    out.replace("'", "'\\''");
    return "'" + out + "'";
}

QString darlingMacOBloxUserDir(const QString &darlingPrefix) {
    const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
    return darlingPrefix + "/Users/" + user + "/Library/MacOBlox";
}

QString findDarlingBin() {
    const QString inPath = QStandardPaths::findExecutable("darling");
    if (!inPath.isEmpty()) return inPath;
    const QStringList searchPaths = {
        QDir::homePath() + "/.local/bin",
        "/usr/local/bin",
        "/usr/bin"
    };
    return QStandardPaths::findExecutable("darling", searchPaths);
}

} // namespace

DarlingController::DarlingController(SettingsManager *settingsMgr, QObject *parent)
    : QObject(parent),
      m_settingsMgr(settingsMgr),
      m_gameProc(new QProcess(this)),
      m_nam(new QNetworkAccessManager(this)) {
    m_cpuThreads = std::max(2, QThread::idealThreadCount());
    m_vramBytes = readSysfsVramBytes();

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
        stopDnsForwarder();
        m_inActivePlace = false;
        bool crashed = (!m_manualStop && (status == QProcess::CrashExit || exitCode != 0));
        emit gameFinished(exitCode, crashed);
        emit sessionStateChanged();
    });
}

DarlingController::~DarlingController() {
    stopDnsForwarder();
    if (m_gameProc && m_gameProc->state() != QProcess::NotRunning) {
        m_gameProc->terminate();
        m_gameProc->waitForFinished(1000);
    }
}

void DarlingController::detectHardwareAsync() {
    m_vramBytes = readSysfsVramBytes();
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

QString DarlingController::shimSourceDir() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::currentPath() + "/shims",
        QDir::currentPath(),
        appDir + "/../shims",
        appDir + "/shims",
        appDir + "/../share/mqblox/shims",
        QDir::homePath() + "/.local/share/mqblox/shims",
        "/usr/local/share/mqblox/shims",
        "/usr/share/mqblox/shims"
    };
    for (const QString &dir : candidates) {
        if (QFile::exists(dir + "/libMacOBloxShims.m") && QFile::exists(dir + "/audio_hal.c")) {
            return QFileInfo(dir).canonicalFilePath();
        }
    }
    return QDir::currentPath() + "/shims";
}

QString DarlingController::shimDylibPath() const {
    const QString inData = m_settingsMgr->macobloxDataDir() + "/libMacOBloxShims.dylib";
    if (QFile::exists(inData)) return inData;
    const QString inDataBuild = m_settingsMgr->macobloxDataDir() + "/build/libMacOBloxShims.dylib";
    if (QFile::exists(inDataBuild)) return inDataBuild;
    const QString inShimBuild = shimSourceDir() + "/build/libMacOBloxShims.dylib";
    if (QFile::exists(inShimBuild)) return inShimBuild;
    const QString inShimDir = shimSourceDir() + "/libMacOBloxShims.dylib";
    if (QFile::exists(inShimDir)) return inShimDir;
    const QString inUsrLib = "/usr/lib/mqblox/libMacOBloxShims.dylib";
    if (QFile::exists(inUsrLib)) return inUsrLib;
    return inData;
}

EnvironmentStatus DarlingController::checkEnvironment() const {
    EnvironmentStatus st;
    const QStringList searchPaths = {
        QDir::homePath() + "/.local/bin",
        "/usr/local/bin",
        "/usr/bin",
        QDir::homePath() + "/.local/share/flatpak/app/xyz.narez.MacOBlox/current/active/files/bin"
    };
    st.hasDarling = !QStandardPaths::findExecutable("darling").isEmpty() ||
                    !QStandardPaths::findExecutable("darling", searchPaths).isEmpty();
    st.hasClang = !QStandardPaths::findExecutable("clang").isEmpty();
    st.hasLld = !QStandardPaths::findExecutable("ld.lld").isEmpty() ||
                !QStandardPaths::findExecutable("ld64.lld").isEmpty() ||
                !QStandardPaths::findExecutable("ld64.lld", searchPaths).isEmpty();
    st.hasUnzip = !QStandardPaths::findExecutable("unzip").isEmpty();
    st.hasShimDylib = QFile::exists(shimDylibPath());

    const QString appBin = m_settingsMgr->robloxAppDir() + "/Contents/MacOS/RobloxPlayer";
    st.hasRobloxApp = QFileInfo(appBin).isExecutable();
    st.installedRobloxVersion = readInstalledRobloxVersion(m_settingsMgr->robloxAppDir());
    st.vramBytes = m_vramBytes > 0 ? m_vramBytes : readSysfsVramBytes();
    const double vramGb = static_cast<double>(st.vramBytes) / (1024.0 * 1024.0 * 1024.0);
    st.gpuSummary = QString("%1 (%2 ГБ VRAM)").arg(m_gpuModel).arg(vramGb, 0, 'f', 1);

    const QString xdgSession = qEnvironmentVariable("XDG_SESSION_TYPE", "unknown");
    const QString waylandDisp = qEnvironmentVariable("WAYLAND_DISPLAY");
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", "Linux");
    if (!waylandDisp.isEmpty() || xdgSession.compare("wayland", Qt::CaseInsensitive) == 0) {
        st.sessionType = QString("Wayland (%1, %2) -> Darling XWayland (EGL_PLATFORM=x11)")
                             .arg(desktop, waylandDisp.isEmpty() ? "wayland" : waylandDisp);
    } else {
        st.sessionType = QString("X11 (%1)").arg(desktop);
    }
    return st;
}

QString DarlingController::collectDiagnosticsReport() const {
    const EnvironmentStatus st = checkEnvironment();
    const AppSettings &s = m_settingsMgr->settings();
    QStringList out;
    out << "=== Отчёт диагностики MqBlox × MacOBlox ===";
    out << QString("Дата: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out << QString("Версия: MqBlox v36.0 (Schema v%1, MacOBlox Darling Backend)").arg(s.schemaVersion);
    out << QString("ОС / Ядро: %1 (%2 %3)")
               .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion());
    out << QString("Процессор: %1 (%2 потоков)").arg(m_cpuModel).arg(m_cpuThreads);
    out << QString("Видеокарта и VRAM: %1").arg(st.gpuSummary);
    out << QString("Сессия отображения: %1").arg(st.sessionType);
    out << QString("Darling установлен: %1").arg(st.hasDarling ? "Да" : "Нет");
    out << QString("Инструменты сборки: clang=%1, ld.lld=%2, unzip=%3")
               .arg(st.hasClang ? "Да" : "Нет", st.hasLld ? "Да" : "Нет", st.hasUnzip ? "Да" : "Нет");
    out << QString("Шим libMacOBloxShims.dylib: %1 (%2)")
               .arg(st.hasShimDylib ? "Собран" : "Не собран", shimDylibPath());
    out << QString("Клиент RobloxPlayer.app: %1 (версия: %2)")
               .arg(st.hasRobloxApp ? "Установлен" : "Не установлен",
                    st.installedRobloxVersion.isEmpty() ? "н/д" : st.installedRobloxVersion);
    out << QString("Сессия авторизации (Cookies.plist): %1")
               .arg(isSignedIn() ? "Присутствует (токен не читается лаунчером)" : "Не авторизована");
    out << QString("Параметры: Target FPS=%1 | VSyncOff=%2 | MSAA=%3 | Мышь=%4% | DNS=#%5")
               .arg(s.unlimitedFps ? 9999 : s.targetFps)
               .arg(s.vsyncOff ? "Да" : "Нет")
               .arg(static_cast<int>(s.msaa))
               .arg(s.mouseSensitivityPct)
               .arg(s.dnsProvider);
    out << "";
    out << "--- Последние ошибки и предупреждения ---";
    if (m_recentErrors.isEmpty()) {
        out << "(Ошибок не зафиксировано)";
    } else {
        for (const QString &err : m_recentErrors) {
            out << "  " + err;
        }
    }
    return out.join('\n');
}

bool DarlingController::isSignedIn() const {
    const QString cookiesPath =
        darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir()) + "/Cookies.plist";
    QFile f(cookiesPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    // Privacy policy: check only key existence, never parse/log/transmit cookie value.
    return f.readAll().contains("<string>.ROBLOSECURITY</string>");
}

void DarlingController::logoutAsync() {
    const QString hostDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
    (void)QtConcurrent::run([this, hostDir, user]() {
        const QString darlingBin = findDarlingBin();
        if (!darlingBin.isEmpty()) {
            QProcess p;
            p.start(darlingBin, {"shell", "/bin/rm", "-rf",
                                 "/Users/" + user + "/Library/MacOBlox/Cookies.plist",
                                 "/Users/" + user + "/Library/MacOBlox/Keychain"});
            p.waitForFinished(4000);
        }
        QFile::remove(hostDir + "/Cookies.plist");
        QDir(hostDir + "/Keychain").removeRecursively();
        QMetaObject::invokeMethod(this, [this]() {
            emit logGenerated("[🔐] Локальная сессия Roblox внутри Darling очищена.");
            emit sessionStateChanged();
        }, Qt::QueuedConnection);
    });
}

QString DarlingController::buildDarlingLaunchScript(const QString &targetUri) const {
    const AppSettings &s = m_settingsMgr->settings();
    const qint64 vram = m_vramBytes > 0 ? m_vramBytes : readSysfsVramBytes();

    const QString appExecutableDir = "/Volumes/SystemRoot" + m_settingsMgr->robloxAppDir() + "/Contents/MacOS";
    const QString dylibFile = QFileInfo(shimDylibPath()).absoluteFilePath();
    const QString dylibDir = "/Volumes/SystemRoot" + QFileInfo(dylibFile).absolutePath();
    const QString dylibInDarling = "/Volumes/SystemRoot" + dylibFile;

    QStringList envExports;
    envExports << "export DYLD_FORCE_FLAT_NAMESPACE=1";
    envExports << QString("export DYLD_INSERT_LIBRARIES=%1").arg(shellSingleQuote(dylibInDarling));
    envExports << QString("export DYLD_LIBRARY_PATH=%1:%2")
                      .arg(shellSingleQuote(dylibDir), shellSingleQuote(appExecutableDir));
    envExports << QString("export MACOBLOX_VRAM_BYTES=%1").arg(vram);
    envExports << QString("export MACOBLOX_VSYNC=%1").arg((s.vsyncOff || s.unlimitedFps) ? "0" : "1");

    if (s.mouseSensitivityPct != 100) {
        const double sens = static_cast<double>(s.mouseSensitivityPct) / 100.0;
        envExports << QString("export MACOBLOX_MOUSE_SENSITIVITY=%1").arg(sens, 0, 'f', 2);
    }
    if (!s.hideMenuBar) {
        envExports << "export MACOBLOX_HIDE_MENU_BAR=0";
    }
    if (m_dnsPort > 0) {
        envExports << QString("export MACOBLOX_DNS=127.0.0.1:%1").arg(m_dnsPort);
    }
    envExports << "export MACOBLOX_WM_CLASS=mqblox";
    if (s.fpsLog || s.showHudFps) envExports << "export MACOBLOX_FPS_LOG=1";
    if (s.traceUdp) envExports << "export MACOBLOX_TRACE_UDP=1";
    if (s.traceLock) envExports << "export MACOBLOX_TRACE_LOCK=1";
    if (s.traceGl) envExports << "export MACOBLOX_TRACE_GL=1";
    if (s.diagnosticSignals) envExports << "export MACOBLOX_DIAGNOSTIC_SIGNALS=1";
    const QString audioLogEnv = qEnvironmentVariable("MACOBLOX_AUDIO_LOG");
    if (!audioLogEnv.isEmpty()) {
        envExports << QString("export MACOBLOX_AUDIO_LOG=%1").arg(shellSingleQuote(audioLogEnv));
    }
    const QString traceAudioEnv = qEnvironmentVariable("MACOBLOX_TRACE_AUDIO");
    if (!traceAudioEnv.isEmpty()) {
        envExports << QString("export MACOBLOX_TRACE_AUDIO=%1").arg(shellSingleQuote(traceAudioEnv));
    }

    QString execCmd = "exec ./RobloxPlayer";
    if (!targetUri.trimmed().isEmpty()) {
        execCmd += " " + shellSingleQuote(targetUri.trimmed());
    }

    return QString("cd %1 && %2 && %3")
        .arg(shellSingleQuote(appExecutableDir), envExports.join(" && "), execCmd);
}

QStringList DarlingController::buildFlatpakArgs(const QString &targetUri) const {
    return {"shell", "/bin/bash", "-c", buildDarlingLaunchScript(targetUri)};
}

void DarlingController::startDnsForwarder() {
    stopDnsForwarder();
    const AppSettings &s = m_settingsMgr->settings();
    if (s.dnsProvider <= 0) return;

    switch (s.dnsProvider) {
    case 1: m_dnsUpstreamIp = "9.9.9.9"; break;
    case 2: m_dnsUpstreamIp = "1.1.1.1"; break;
    case 3: m_dnsUpstreamIp = "8.8.8.8"; break;
    case 4: m_dnsUpstreamIp = s.dnsCustom.trimmed().isEmpty() ? "9.9.9.9" : s.dnsCustom.trimmed(); break;
    default: m_dnsUpstreamIp = "9.9.9.9"; break;
    }

    m_dnsUdp = new QUdpSocket(this);
    if (!m_dnsUdp->bind(QHostAddress::LocalHost, 0)) {
        delete m_dnsUdp;
        m_dnsUdp = nullptr;
        return;
    }
    m_dnsPort = m_dnsUdp->localPort();

    connect(m_dnsUdp, &QUdpSocket::readyRead, this, [this]() {
        while (m_dnsUdp && m_dnsUdp->hasPendingDatagrams()) {
            QByteArray query;
            query.resize(static_cast<int>(m_dnsUdp->pendingDatagramSize()));
            QHostAddress senderHost;
            quint16 senderPort = 0;
            m_dnsUdp->readDatagram(query.data(), query.size(), &senderHost, &senderPort);
            if (query.size() < 12) continue;

            const QString upstream = m_dnsUpstreamIp;
            QPointer<QUdpSocket> udpGuard(m_dnsUdp);
            (void)QtConcurrent::run([query, upstream, senderHost, senderPort, udpGuard]() {
                QSslSocket tls;
                tls.setPeerVerifyMode(QSslSocket::VerifyNone);
                tls.connectToHostEncrypted(upstream, 853);
                if (!tls.waitForEncrypted(2500)) return;

                QByteArray framed;
                const quint16 qlen = static_cast<quint16>(query.size());
                framed.append(static_cast<char>((qlen >> 8) & 0xFF));
                framed.append(static_cast<char>(qlen & 0xFF));
                framed.append(query);
                tls.write(framed);
                if (!tls.waitForBytesWritten(1500)) return;

                QByteArray respBuf;
                while (respBuf.size() < 2 && tls.waitForReadyRead(2500)) {
                    respBuf.append(tls.readAll());
                }
                if (respBuf.size() < 2) return;
                const int rlen = (static_cast<unsigned char>(respBuf[0]) << 8) |
                                 static_cast<unsigned char>(respBuf[1]);
                while (respBuf.size() < 2 + rlen && tls.waitForReadyRead(2500)) {
                    respBuf.append(tls.readAll());
                }
                if (respBuf.size() >= 2 + rlen && udpGuard) {
                    const QByteArray payload = respBuf.mid(2, rlen);
                    QMetaObject::invokeMethod(udpGuard, [udpGuard, payload, senderHost, senderPort]() {
                        if (udpGuard) {
                            udpGuard->writeDatagram(payload, senderHost, senderPort);
                        }
                    }, Qt::QueuedConnection);
                }
            });
        }
    });
}

void DarlingController::stopDnsForwarder() {
    m_dnsPort = 0;
    if (m_dnsUdp) {
        m_dnsUdp->close();
        m_dnsUdp->deleteLater();
        m_dnsUdp = nullptr;
    }
}

void DarlingController::clearStaleDarlingProcesses() {
    const uid_t myUid = ::getuid();
    const QString myPrefix = QFileInfo(m_settingsMgr->darlingPrefixDir()).absoluteFilePath();
    QDir procDir("/proc");
    for (const QString &pidStr : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const pid_t pid = static_cast<pid_t>(pidStr.toInt(&ok));
        if (!ok || pid <= 1 || pid == ::getpid()) continue;

        const QString procPath = QString("/proc/%1").arg(pid);
        if (QFileInfo(procPath).ownerId() != myUid) continue;

        QFile commFile(procPath + "/comm");
        if (!commFile.open(QIODevice::ReadOnly)) continue;
        const QString comm = QString::fromUtf8(commFile.readAll()).trimmed();
        const bool isOurBinary = (comm == "RobloxPlayer" || comm == "RobloxCrashHandler" ||
                                  comm == "darlingserver" || comm == "darling");
        if (!isOurBinary) continue;

        const QString cwdTarget = QFile::symLinkTarget(procPath + "/cwd");
        const bool isDeletedCwd = cwdTarget.endsWith(" (deleted)");
        const bool isOurPrefix = !myPrefix.isEmpty() && cwdTarget.startsWith(myPrefix);
        if (isDeletedCwd || (isOurPrefix && (comm == "RobloxPlayer" || comm == "RobloxCrashHandler"))) {
            ::kill(pid, SIGKILL);
        }
    }
}

void DarlingController::launchRoblox(const QString &customUri) {
    m_manualStop = false;
    m_inActivePlace = false;
    m_settingsMgr->saveAll();

    const EnvironmentStatus st = checkEnvironment();
    if (!st.hasDarling) {
        const QString msg = "[⚠️] Darling не установлен в системе. Откройте «Мастер установки и обновления».";
        m_recentErrors.append(msg);
        emit logGenerated(msg);
        emit toastRequested(msg);
        return;
    }
    if (!st.hasRobloxApp) {
        const QString msg = "[⚠️] RobloxPlayer.app не установлен. Запустите «Мастер установки и обновления».";
        m_recentErrors.append(msg);
        emit logGenerated(msg);
        emit toastRequested(msg);
        return;
    }

    clearStaleDarlingProcesses();
    startDnsForwarder();

    const QString targetUri = !customUri.isEmpty()
                                  ? customUri
                                  : SettingsManager::parsePlaceUri(m_settingsMgr->settings().lastPlaceInput);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Darling's CGL uses X11/XWayland; keep Qt6 native on Wayland while setting EGL_PLATFORM=x11 for Darling
    env.insert("EGL_PLATFORM", "x11");
    m_gameProc->setProcessEnvironment(env);

    const QString darlingBin = findDarlingBin().isEmpty() ? QString("darling") : findDarlingBin();
    const QStringList args = buildFlatpakArgs(targetUri);
    emit logGenerated("[🚀] Запуск MacOBlox (Darling): " + darlingBin + " " + args.join(' '));
    m_gameProc->start(darlingBin, args);
}

void DarlingController::stopRobloxAsync() {
    m_manualStop = true;
    m_inActivePlace = false;
    stopDnsForwarder();

    const QString dprefix = m_settingsMgr->darlingPrefixDir();
    (void)QtConcurrent::run([this, dprefix]() {
        const QString darlingBin = findDarlingBin();
        if (!darlingBin.isEmpty()) {
            QProcess killer;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            if (!dprefix.isEmpty()) env.insert("DPREFIX", dprefix);
            killer.setProcessEnvironment(env);
            killer.start(darlingBin, {"shell", "/usr/bin/killall", "-9", "RobloxPlayer", "RobloxCrashHandler"});
            killer.waitForFinished(3000);
        }
        clearStaleDarlingProcesses();
        QMetaObject::invokeMethod(this, [this]() {
            if (isGameRunning()) {
                m_gameProc->terminate();
                if (!m_gameProc->waitForFinished(1200)) {
                    m_gameProc->kill();
                }
            }
            emit logGenerated("[🛑] Сессия RobloxPlayer внутри Darling завершена.");
        }, Qt::QueuedConnection);
    });
}

void DarlingController::restartDarlingAsync() {
    stopDnsForwarder();
    emit logGenerated("[🔄] Перезапуск контейнера Darling...");
    const QString dprefix = m_settingsMgr->darlingPrefixDir();
    (void)QtConcurrent::run([this, dprefix]() {
        const QString darlingBin = findDarlingBin();
        if (!darlingBin.isEmpty()) {
            QProcess p;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            if (!dprefix.isEmpty()) env.insert("DPREFIX", dprefix);
            p.setProcessEnvironment(env);
            p.start(darlingBin, {"shutdown"});
            p.waitForFinished(4000);
        }
        clearStaleDarlingProcesses();
        QMetaObject::invokeMethod(this, [this]() {
            emit logGenerated("[✓] Контейнер Darling перезапущен и очищен.");
            emit toastRequested("🔄 Контейнер Darling перезапущен");
        }, Qt::QueuedConnection);
    });
}

void DarlingController::saveCurrentSessionToSlotAsync() {
    const QString macobloxDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString slotDir =
        m_settingsMgr->accountsDir() + "/" + m_settingsMgr->activeAccountId() + "/MacOBlox";
    (void)QtConcurrent::run([macobloxDir, slotDir]() {
        if (QDir(macobloxDir).exists()) {
            QDir(slotDir).removeRecursively();
            copyDirectoryContent(macobloxDir, slotDir);
        }
    });
}

void DarlingController::switchAccountSlotAsync(const QString &targetAccountId) {
    stopRobloxAsync();
    const QString macobloxDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString oldSlot =
        m_settingsMgr->accountsDir() + "/" + m_settingsMgr->activeAccountId() + "/MacOBlox";
    const QString newSlot =
        m_settingsMgr->accountsDir() + "/" + targetAccountId + "/MacOBlox";

    m_settingsMgr->setActiveAccountId(targetAccountId);
    m_settingsMgr->saveAll();

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, targetAccountId]() {
        watcher->deleteLater();
        emit accountSwitchCompleted(targetAccountId);
        emit sessionStateChanged();
    });

    watcher->setFuture(QtConcurrent::run([macobloxDir, oldSlot, newSlot]() {
        if (QDir(macobloxDir).exists()) {
            QDir(oldSlot).removeRecursively();
            copyDirectoryContent(macobloxDir, oldSlot);
        }
        QDir(macobloxDir).removeRecursively();
        if (QDir(newSlot).exists()) {
            copyDirectoryContent(newSlot, macobloxDir);
        } else {
            QDir().mkpath(macobloxDir);
        }
    }));
}

void DarlingController::prepareDarlingPrefixSync() {
    const QString prefix = m_settingsMgr->darlingPrefixDir();
    if (!QDir(prefix).exists()) return;

    const uid_t myUid = ::getuid();
    const QString absPrefix = QFileInfo(prefix).absoluteFilePath();
    QDir procDir("/proc");
    for (const QString &pidStr : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const pid_t pid = static_cast<pid_t>(pidStr.toInt(&ok));
        if (!ok || pid <= 1) continue;
        const QString procPath = QString("/proc/%1").arg(pid);
        if (QFileInfo(procPath).ownerId() != myUid) continue;
        QFile commFile(procPath + "/comm");
        if (!commFile.open(QIODevice::ReadOnly)) continue;
        if (QString::fromUtf8(commFile.readAll()).trimmed() == "darlingserver") {
            const QString cwd = QFile::symLinkTarget(procPath + "/cwd");
            if (cwd.startsWith(absPrefix)) return;
        }
    }

    const QString fwSrc = shimSourceDir() + "/frameworks";
    const QString fwDst = prefix + "/System/Library/Frameworks";
    if (QDir(fwSrc).exists() && !QStandardPaths::findExecutable("clang").isEmpty()) {
        struct StubFw { const char *name; const char *src; };
        const StubFw kStubs[] = {
            {"CoreML", "CoreML.m"},
            {"CoreHaptics", "CoreHaptics.m"},
            {"DeviceCheck", "DeviceCheck.m"}
        };
        for (const auto &fw : kStubs) {
            const QString dir = QString("%1/%2.framework/Versions/A").arg(fwDst, fw.name);
            const QString bin = dir + "/" + fw.name;
            if (QFile::exists(bin)) continue;
            QDir().mkpath(dir);
            QProcess clang;
            clang.start("clang", {
                "-target", "x86_64-apple-darwin20",
                "-dynamiclib", "-fuse-ld=lld", "-nostdlib",
                "-Wl,-install_name,/System/Library/Frameworks/" + QString(fw.name) +
                    ".framework/Versions/A/" + QString(fw.name),
                "-Wl,-undefined,dynamic_lookup",
                fwSrc + "/" + fw.src,
                "-o", bin
            });
            clang.waitForFinished(10000);
        }
    }
}

void DarlingController::runSetupOrUpdateWizardAsync() {
    if (m_wizardBusy) return;
    m_wizardBusy = true;

    emit wizardProgress(5, "Проверка инструментов (clang, ld.lld, unzip, darling)...");
    const EnvironmentStatus st = checkEnvironment();
    if (!st.hasClang || !st.hasLld || !st.hasUnzip) {
        m_wizardBusy = false;
        const QString msg = QString("Отсутствуют пакеты сборки: %1%2%3")
                                .arg(!st.hasClang ? "clang " : "",
                                     !st.hasLld ? "lld " : "",
                                     !st.hasUnzip ? "unzip" : "");
        m_recentErrors.append(msg);
        emit logGenerated("[Мастер] " + msg);
        emit wizardFinished(false, msg);
        return;
    }

    emit wizardProgress(18, "Компиляция шима libMacOBloxShims.dylib (включая audio_hal.c)...");
    const QString srcDir = shimSourceDir();
    const QString buildScript = srcDir + "/build_debug_shim.sh";
    if (!QFile::exists(buildScript)) {
        m_wizardBusy = false;
        const QString msg = "Не найден скрипт сборки шимов: " + buildScript;
        m_recentErrors.append(msg);
        emit wizardFinished(false, msg);
        return;
    }

    if (m_wizardProc) m_wizardProc->deleteLater();
    m_wizardProc = new QProcess(this);
    m_wizardProc->setWorkingDirectory(srcDir);
    m_wizardProc->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment wizEnv = QProcessEnvironment::systemEnvironment();
    wizEnv.insert("MACOBLOX_BUILD_DIR", m_settingsMgr->macobloxDataDir());
    m_wizardProc->setProcessEnvironment(wizEnv);

    connect(m_wizardProc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(m_wizardProc->readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) emit logGenerated(out);

        const QString dstDylib = m_settingsMgr->macobloxDataDir() + "/libMacOBloxShims.dylib";
        if (code != 0 || !QFile::exists(dstDylib)) {
            m_wizardBusy = false;
            const QString msg = "Ошибка сборки libMacOBloxShims.dylib";
            m_recentErrors.append(msg);
            emit wizardFinished(false, msg);
            return;
        }

        emit wizardProgress(40, "Подготовка префикса Darling и запрос версии Roblox MacPlayer...");
        prepareDarlingPrefixSync();
        stepDownloadOrUpdateRoblox();
    });

    m_wizardProc->start("/bin/bash", {buildScript});
}

void DarlingController::stepDownloadOrUpdateRoblox() {
    QNetworkRequest req(QUrl("https://clientsettingscdn.roblox.com/v2/client-version/MacPlayer"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "MacOBlox/1.0");
    QNetworkReply *verReply = m_nam->get(req);

    connect(verReply, &QNetworkReply::finished, this, [this, verReply]() {
        verReply->deleteLater();
        if (verReply->error() != QNetworkReply::NoError) {
            m_wizardBusy = false;
            const QString msg = "Ошибка сети при проверке версии Roblox: " + verReply->errorString();
            m_recentErrors.append(msg);
            emit wizardFinished(false, msg);
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(verReply->readAll()).object();
        const QString version = obj.value("version").toString();
        const QString upload = obj.value("clientVersionUpload").toString();
        if (upload.isEmpty()) {
            m_wizardBusy = false;
            emit wizardFinished(false, "Официальный CDN вернул пустой clientVersionUpload.");
            return;
        }

        const QString installedVer = readInstalledRobloxVersion(m_settingsMgr->robloxAppDir());
        if (!installedVer.isEmpty() && installedVer == version &&
            QFile::exists(m_settingsMgr->robloxAppDir() + "/Contents/MacOS/RobloxPlayer")) {
            m_settingsMgr->saveAll();
            m_wizardBusy = false;
            const QString msg = QString("Шим собран, клиент Roblox %1 уже актуален.").arg(version);
            emit wizardProgress(100, msg);
            emit wizardFinished(true, msg);
            return;
        }

        const QUrl zipUrl(QString("https://setup.rbxcdn.com/mac/%1-RobloxPlayer.zip").arg(upload));
        emit wizardProgress(50, QString("Скачивание Roblox %1 (%2)...").arg(version, upload));

        const QString zipPath = m_settingsMgr->macobloxDataDir() + "/RobloxPlayer.zip.part";
        QFile *outFile = new QFile(zipPath, this);
        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            delete outFile;
            m_wizardBusy = false;
            emit wizardFinished(false, "Не удалось создать файл загрузки: " + zipPath);
            return;
        }

        QNetworkRequest dlReq(zipUrl);
        dlReq.setHeader(QNetworkRequest::UserAgentHeader, "MacOBlox/1.0");
        QNetworkReply *dlReply = m_nam->get(dlReq);

        connect(dlReply, &QNetworkReply::readyRead, this, [dlReply, outFile]() {
            outFile->write(dlReply->readAll());
        });

        connect(dlReply, &QNetworkReply::downloadProgress, this,
                [this, version](qint64 received, qint64 total) {
                    if (total > 0) {
                        const int pct = 50 + static_cast<int>((received * 40) / total);
                        const double mbRecv = static_cast<double>(received) / (1024.0 * 1024.0);
                        const double mbTot = static_cast<double>(total) / (1024.0 * 1024.0);
                        emit wizardProgress(
                            std::clamp(pct, 50, 90),
                            QString("Загрузка Roblox %1: %2 / %3 МБ")
                                .arg(version)
                                .arg(mbRecv, 0, 'f', 1)
                                .arg(mbTot, 0, 'f', 1));
                    }
                });

        connect(dlReply, &QNetworkReply::finished, this,
                [this, dlReply, outFile, zipPath, version]() {
                    outFile->write(dlReply->readAll());
                    outFile->close();
                    outFile->deleteLater();
                    dlReply->deleteLater();

                    if (dlReply->error() != QNetworkReply::NoError) {
                        QFile::remove(zipPath);
                        m_wizardBusy = false;
                        const QString msg = "Ошибка скачивания RobloxPlayer.zip: " + dlReply->errorString();
                        m_recentErrors.append(msg);
                        emit wizardFinished(false, msg);
                        return;
                    }

                    emit wizardProgress(92, "Распаковка RobloxPlayer.app и применение FastFlags...");
                    const QString dataDir = m_settingsMgr->macobloxDataDir();
                    const QString appDir = m_settingsMgr->robloxAppDir();

                    (void)QtConcurrent::run([this, zipPath, dataDir, appDir, version]() {
                        const QString extractDir = dataDir + "/.extract_tmp";
                        QDir(extractDir).removeRecursively();
                        QDir().mkpath(extractDir);

                        QProcess unzipProc;
                        unzipProc.start("unzip", {"-q", zipPath, "-d", extractDir});
                        const bool unzipOk = unzipProc.waitForFinished(120000) && unzipProc.exitCode() == 0;
                        QFile::remove(zipPath);

                        if (!unzipOk || !QDir(extractDir + "/RobloxPlayer.app").exists()) {
                            QDir(extractDir).removeRecursively();
                            QMetaObject::invokeMethod(this, [this]() {
                                m_wizardBusy = false;
                                emit wizardFinished(false, "Не удалось распаковать архив RobloxPlayer.zip.");
                            }, Qt::QueuedConnection);
                            return;
                        }

                        if (QDir(appDir).exists()) {
                            const QString oldVer = readInstalledRobloxVersion(appDir);
                            const QString backupDir =
                                dataDir + "/backups/RobloxPlayer-" +
                                (oldVer.isEmpty() ? "prev" : oldVer) + ".app";
                            QDir(backupDir).removeRecursively();
                            QDir().mkpath(dataDir + "/backups");
                            QDir().rename(appDir, backupDir);
                        }

                        QDir().rename(extractDir + "/RobloxPlayer.app", appDir);
                        QDir(extractDir).removeRecursively();

                        QMetaObject::invokeMethod(this, [this, version]() {
                            m_settingsMgr->saveAll();
                            m_wizardBusy = false;
                            const QString msg = QString("Roblox %1 и шимы MacOBlox успешно установлены!").arg(version);
                            emit wizardProgress(100, msg);
                            emit wizardFinished(true, msg);
                        }, Qt::QueuedConnection);
                    });
                });
    });
}

void DarlingController::handleProcessOutput(const QString &chunk) {
    const QStringList &markers = m_settingsMgr->settings().leaveMarkers;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const QString &rawLine : chunk.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.contains("ROBLOSECURITY", Qt::CaseInsensitive) ||
            line.contains("Bearer ", Qt::CaseInsensitive)) {
            continue;
        }
        if (line.contains("error", Qt::CaseInsensitive) ||
            line.contains("fail", Qt::CaseInsensitive) ||
            line.contains("crash", Qt::CaseInsensitive) ||
            line.contains("stall", Qt::CaseInsensitive)) {
            m_recentErrors.append(line);
            while (m_recentErrors.size() > 30) {
                m_recentErrors.removeFirst();
            }
        }
        emit logGenerated(line);

        if (line.contains("Joining game") || line.contains("Connecting to UDMUX") ||
            line.contains("Connection accepted")) {
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
