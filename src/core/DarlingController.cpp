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
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QTimeZone>
#include <QUdpSocket>
#include <QtConcurrent>
#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
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

void ensureRobloxExecPermissions(const QString &robloxAppDir) {
    const QString macOsDir = robloxAppDir + "/Contents/MacOS";
    const QDir dir(macOsDir);
    if (!dir.exists()) return;
    const auto entries = dir.entryInfoList(QDir::Files);
    for (const QFileInfo &info : entries) {
        QFile::setPermissions(info.filePath(),
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
            QFileDevice::ReadGroup | QFileDevice::ExeGroup |
            QFileDevice::ReadOther | QFileDevice::ExeOther);
    }
}

} // namespace

QString DarlingController::darlingMacOBloxUserDir(const QString &darlingPrefix) {
    const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
    return darlingPrefix + "/Users/" + user + "/Library/MacOBlox";
}

DarlingController::DarlingController(SettingsManager *settingsMgr, QObject *parent)
    : QObject(parent),
      m_settingsMgr(settingsMgr),
      m_gameProc(new QProcess(this)),
      m_nam(new QNetworkAccessManager(this)) {
    m_cpuThreads = std::max(2, QThread::idealThreadCount());
    m_vramBytes = readSysfsVramBytes();

    QFile cpuInfo("/proc/cpuinfo");
    if (cpuInfo.open(QIODevice::ReadOnly)) {
        for (const QString &line : QString::fromUtf8(cpuInfo.readAll()).split('\n')) {
            if (line.startsWith("model name") && m_cpuModel == "CPU x86_64") {
                m_cpuModel = line.section(':', 1).trimmed();
            } else if (line.startsWith("flags")) {
                const QStringList flags = line.section(':', 1).trimmed().split(' ', Qt::SkipEmptyParts);
                m_cpuAvx = flags.contains("avx");
                m_cpuAvx2 = flags.contains("avx2");
            }
        }
    }

    m_launchWatchdog = new QTimer(this);
    m_launchWatchdog->setSingleShot(true);
    connect(m_launchWatchdog, &QTimer::timeout, this, [this]() {
        if (!isRobloxProcessAlive() && m_gameProc && m_gameProc->state() != QProcess::NotRunning) {
            emit logGenerated("[⚠️] Превышено время ожидания запуска Darling / RobloxPlayer (35 с). Сброс зависшего префикса...");
            emit toastRequested("⚠️ Превышено время запуска Roblox (35 с). Префикс сброшен.");
            m_gameProc->kill();
            clearStaleDarlingProcesses();
            emit gameFinished(124, true);
        }
    });

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
        if (m_launchWatchdog && m_launchWatchdog->isActive()) {
            m_launchWatchdog->stop();
        }

        auto finishSession = [this, exitCode, status]() {
            if (m_procMonitorTimer) {
                m_procMonitorTimer->stop();
                m_procMonitorTimer->deleteLater();
            }
            m_lastExitTime = QDateTime::currentDateTime();
            m_lastExitCode = exitCode;
            m_lastExitStatus = status;
            clearStaleDarlingProcesses();
            stopDnsForwarder();
            m_inActivePlace = false;
            bool crashed = (!m_manualStop && (status == QProcess::CrashExit || exitCode != 0));
            m_lastCrashed = crashed;
            emit gameFinished(exitCode, crashed);
            emit sessionStateChanged();
        };

        if (isRobloxProcessAlive()) {
            if (m_procMonitorTimer) {
                m_procMonitorTimer->stop();
                m_procMonitorTimer->deleteLater();
            }
            m_procMonitorTimer = new QTimer(this);
            connect(m_procMonitorTimer, &QTimer::timeout, this, [this, finishSession]() {
                if (!isRobloxProcessAlive()) {
                    if (m_procMonitorTimer) {
                        m_procMonitorTimer->stop();
                        m_procMonitorTimer->deleteLater();
                    }
                    finishSession();
                }
            });
            m_procMonitorTimer->start(1000);
        } else {
            finishSession();
        }
    });
}

DarlingController::~DarlingController() {
    stopDnsForwarder();
    if (m_glxProc && m_glxProc->state() != QProcess::NotRunning) {
        m_glxProc->kill();
        m_glxProc->waitForFinished(50);
    }
    if (m_lspciProc && m_lspciProc->state() != QProcess::NotRunning) {
        m_lspciProc->kill();
        m_lspciProc->waitForFinished(50);
    }
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
            } else if (line.startsWith("flags")) {
                const QStringList flags = line.section(':', 1).trimmed().split(' ', Qt::SkipEmptyParts);
                m_cpuAvx = flags.contains("avx");
                m_cpuAvx2 = flags.contains("avx2");
            }
        }
    }

    // Direct sysfs DRM vendor fallback
    QDir drmDir("/sys/class/drm");
    for (const QString &card : drmDir.entryList({"card*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (card.contains('-')) continue;
        QFile vFile(drmDir.filePath(card + "/device/vendor"));
        if (vFile.open(QIODevice::ReadOnly)) {
            const QString vId = QString::fromLatin1(vFile.readAll()).trimmed().toLower();
            if (vId == "0x10de") { m_gpuVendor = "nvidia"; if (m_gpuModel == "GPU Linux") m_gpuModel = "NVIDIA GPU"; break; }
            if (vId == "0x1002") { m_gpuVendor = "amd"; if (m_gpuModel == "GPU Linux") m_gpuModel = "AMD Radeon"; break; }
            if (vId == "0x8086") { m_gpuVendor = "intel"; if (m_gpuModel == "GPU Linux") m_gpuModel = "Intel Graphics"; break; }
        }
    }

    if (m_glxProc && m_glxProc->state() != QProcess::NotRunning) {
        m_glxProc->kill();
    }
    m_glxProc = new QProcess(this);
    connect(m_glxProc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0 && m_glxProc) {
            const QString glxOut = QString::fromUtf8(m_glxProc->readAllStandardOutput());
            for (const QString &rawLine : glxOut.split('\n')) {
                const QString line = rawLine.trimmed();
                if (line.startsWith("OpenGL renderer string:")) {
                    m_glRenderer = line.section(':', 1).trimmed();
                } else if (line.startsWith("OpenGL core profile version string:")) {
                    m_glCoreVersion = line.section(':', 1).trimmed();
                } else if (line.startsWith("Version:") && m_mesaVersion.isEmpty()) {
                    m_mesaVersion = line.section(':', 1).trimmed();
                }
            }
        }
        if (m_glxProc) m_glxProc->deleteLater();
    });
    connect(m_glxProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_glxProc) m_glxProc->deleteLater();
    });
    m_glxProc->start("glxinfo", {"-B"});

    if (m_lspciProc && m_lspciProc->state() != QProcess::NotRunning) {
        m_lspciProc->kill();
    }
    m_lspciProc = new QProcess(this);
    connect(m_lspciProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_lspciProc) m_lspciProc->deleteLater();
        emit hardwareDetected();
    });
    connect(m_lspciProc, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (m_lspciProc) {
            for (const QString &line : QString::fromUtf8(m_lspciProc->readAllStandardOutput()).split('\n')) {
                if (line.contains("VGA") || line.contains("3D controller") || line.contains("Display controller")) {
                    m_gpuModel = line.section(':', 2).trimmed();
                    QString low = m_gpuModel.toLower();
                    if (low.contains("nvidia") || low.contains("geforce")) m_gpuVendor = "nvidia";
                    else if (low.contains("amd") || low.contains("radeon") || low.contains("ati")) m_gpuVendor = "amd";
                    else if (low.contains("intel")) m_gpuVendor = "intel";
                    break;
                }
            }
            m_lspciProc->deleteLater();
        }
        emit hardwareDetected();
    });
    m_lspciProc->start("lspci", {});
}

QString DarlingController::shimSourceDir() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString xdgData = qEnvironmentVariable("XDG_DATA_HOME");
    const QString baseData = !xdgData.trimmed().isEmpty() ? xdgData.trimmed() : (QDir::homePath() + "/.local/share");
    QStringList candidates = {
        QDir::currentPath() + "/shims",
        QDir::currentPath(),
        appDir + "/../shims",
        appDir + "/shims",
        appDir + "/../share/mqblox/shims",
        baseData + "/mqblox/shims",
        "/usr/local/share/mqblox/shims",
        "/usr/share/mqblox/shims"
    };
    const QString xdgDataDirs = qEnvironmentVariable("XDG_DATA_DIRS");
    const QStringList extraDirs = !xdgDataDirs.trimmed().isEmpty()
        ? xdgDataDirs.split(':', Qt::SkipEmptyParts)
        : QStringList{"/usr/local/share", "/usr/share"};
    for (const QString &d : extraDirs) {
        candidates.append(d + "/mqblox/shims");
    }
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
    const QString xdgData = qEnvironmentVariable("XDG_DATA_HOME");
    const QString baseData = !xdgData.trimmed().isEmpty() ? xdgData.trimmed() : (QDir::homePath() + "/.local/share");
    const QStringList searchPaths = {
        QDir::homePath() + "/.local/bin",
        "/usr/local/bin",
        "/usr/bin",
        baseData + "/flatpak/app/xyz.narez.MacOBlox/current/active/files/bin"
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

QString DarlingController::readLatestRobloxLogTail(int maxLines) const {
    const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
    const QString prefix = m_settingsMgr->darlingPrefixDir();
    const QString logsDir = prefix + "/Users/" + user + "/Library/Logs/Roblox";
    QDir dir(logsDir);
    if (!dir.exists()) {
        return "(Журнал Roblox не найден: каталог отсутствует)";
    }

    const QFileInfoList files = dir.entryInfoList({"*_Player_*_last.log", "*.log"},
                                                  QDir::Files, QDir::Time);
    if (files.isEmpty()) {
        return "(Файлы журнала Roblox отсутствуют)";
    }

    const QString logPath = files.first().absoluteFilePath();
    QFile f(logPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("(Не удалось открыть журнал: %1)").arg(logPath);
    }

    QTextStream in(&f);
    QStringList allLines;
    while (!in.atEnd()) {
        allLines.append(in.readLine());
    }
    f.close();

    const int total = static_cast<int>(allLines.size());
    const int start = std::max(0, total - maxLines);
    QStringList tail = allLines.mid(start);

    static const QRegularExpression apiKeyRegex(R"(apiKey=[^&\s"']+)");
    static const QRegularExpression cookieRegex(R"(\.ROBLOSECURITY=[^;\s"']+)");
    static const QRegularExpression tokenRegex(R"(securityToken=[^&\s"']+)");

    for (QString &line : tail) {
        line.replace(apiKeyRegex, "apiKey=<REDACTED>");
        line.replace(cookieRegex, ".ROBLOSECURITY=<REDACTED>");
        line.replace(tokenRegex, "securityToken=<REDACTED>");
    }

    return QString("--- Последние %1 строк журнала Roblox (%2) ---\n%3")
        .arg(tail.size())
        .arg(files.first().fileName(), tail.join('\n'));
}

QString DarlingController::collectDiagnosticsReport() const {
    const EnvironmentStatus st = checkEnvironment();
    const AppSettings &s = m_settingsMgr->settings();
    QStringList out;
    out << "=== Отчёт диагностики MqBlox × MacOBlox ===";
    out << QString("Дата формирования: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out << QString("Версия: MqBlox v36.0 (Schema v%1, MacOBlox Darling Backend)").arg(s.schemaVersion);

    const QString tz = QTimeZone::systemTimeZone().id();
    if (m_lastLaunchTime.isValid()) {
        out << QString("Время последнего запуска: %1 (%2)")
                   .arg(m_lastLaunchTime.toString(Qt::ISODate), tz);
        if (m_lastExitTime.isValid()) {
            out << QString("Время завершения/вылета: %1 (%2)")
                       .arg(m_lastExitTime.toString(Qt::ISODate), tz);
        }
        if (m_lastCrashed) {
            const int sig = (m_lastExitCode > 128) ? (m_lastExitCode - 128) : 0;
            const QString sigStr = (sig == 11) ? "SIGSEGV (11)" : (sig == 4) ? "SIGILL (4)" : (sig == 6) ? "SIGABRT (6)" : QString::number(sig);
            out << QString("Статус последнего запуска: 💥 ВЫЛЕТ (код выхода: %1, сигнал: %2)")
                       .arg(m_lastExitCode).arg(sigStr);
        } else {
            out << QString("Статус последнего запуска: 🟢 Завершился штатно (код: %1)")
                       .arg(m_lastExitCode);
        }
    } else {
        out << "Статус последнего запуска: (Запуск ещё не выполнялся в текущей сессии)";
    }

    out << QString("ОС / Ядро: %1 (%2 %3)")
               .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion());
    out << QString("Процессор: %1 (%2 потоков, флаги CPU: avx=%3, avx2=%4)")
               .arg(m_cpuModel)
               .arg(m_cpuThreads)
               .arg(m_cpuAvx ? "Да" : "Нет")
               .arg(m_cpuAvx2 ? "Да" : "Нет");
    if (!m_cpuAvx2) {
        out << "  ⚠️ Аппаратное ограничение CPU: Отсутствует набор инструкций AVX2 (Sandy Bridge / Ivy Bridge). "
               "Клиент RobloxPlayer (0.744+) использует AVX2 (vpbroadcastd/vpermd) для расчёта 3D-деформации мешей при входе в плейсы, вызывая SIGILL (#UD).";
    }
    out << QString("Видеокарта и VRAM: %1").arg(st.gpuSummary);
    if (!m_mesaVersion.isEmpty() || !m_glRenderer.isEmpty()) {
        out << QString("Графика хоста (OpenGL): Mesa %1 | Рендерер: %2 | Core Profile: %3")
                   .arg(m_mesaVersion.isEmpty() ? "н/д" : m_mesaVersion)
                   .arg(m_glRenderer.isEmpty() ? m_gpuModel : m_glRenderer)
                   .arg(m_glCoreVersion.isEmpty() ? "4.1+" : m_glCoreVersion);
    }
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
               .arg(isSignedIn() ? "🟢 Сессия активна (Cookies.plist сохранён в Darling; токен защищён и не считывается лаунчером)" : "Не авторизована");

    QString dnsDesc;
    switch (s.dnsProvider) {
        case 0: dnsDesc = "0 (Системный DNS / прямой)"; break;
        case 1: dnsDesc = "1 (Quad9 DoT)"; break;
        case 2: dnsDesc = "2 (Cloudflare DoT)"; break;
        case 3: dnsDesc = "3 (Google DoT)"; break;
        case 4: dnsDesc = QString("4 (Свой DoT: %1)").arg(s.dnsCustom); break;
        default: dnsDesc = QString("%1 (Неизвестно)").arg(s.dnsProvider); break;
    }
    out << QString("Параметры: Target FPS=%1 | VSyncOff=%2 | MSAA=%3 | Мышь=%4% | DNS=%5")
               .arg(s.unlimitedFps ? 9999 : s.targetFps)
               .arg(s.vsyncOff ? "Да" : "Нет")
               .arg(static_cast<int>(s.msaa))
               .arg(s.mouseSensitivityPct)
               .arg(dnsDesc);
    out << QString("DNS-прокси MqBlox: %1")
               .arg(m_dnsPort > 0 ? QString("🟢 Активен на 127.0.0.1:%1 (upstream: %2)").arg(m_dnsPort).arg(m_dnsUpstreamIp) : "⚪ Не запущен");

    out << "";
    out << "--- Последние ошибки и предупреждения лаунчера ---";
    if (m_recentErrors.isEmpty()) {
        out << "(Ошибок не зафиксировано)";
    } else {
        for (const QString &err : m_recentErrors) {
            out << "  " + err;
        }
    }

    out << "";
    out << "--- Последний вывод Darling / RobloxPlayer (консоль) ---";
    if (m_recentDarlingLog.isEmpty()) {
        out << "(Вывод консоли пуст)";
    } else {
        const int dStart = std::max(0, static_cast<int>(m_recentDarlingLog.size()) - 50);
        for (int i = dStart; i < m_recentDarlingLog.size(); ++i) {
            out << "  " + m_recentDarlingLog.at(i);
        }
    }

    out << "";
    out << readLatestRobloxLogTail(200);

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
    (void)QtConcurrent::run([this, hostDir]() {
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
    const QString hostFwDir = m_settingsMgr->macobloxDataDir() + "/frameworks";
    if (QDir(hostFwDir).exists()) {
        envExports << QString("export DYLD_FRAMEWORK_PATH=%1")
                          .arg(shellSingleQuote("/Volumes/SystemRoot" + hostFwDir));
    }
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
    envExports << "export MACOBLOX_TRACE_EXIT=1";
    if (s.diagnosticSignals) envExports << "export MACOBLOX_TRACE_EXCEPTIONS=1";
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
    QString backendStr = (s.graphicsBackend == GraphicsBackend::Vulkan) ? "vulkan" : "opengl";
    const QString gfxBackendEnv = qEnvironmentVariable("MACOBLOX_GRAPHICS_BACKEND");
    if (!gfxBackendEnv.isEmpty()) {
        static const QRegularExpression safeIdent("^[a-zA-Z0-9_-]+$");
        if (safeIdent.match(gfxBackendEnv).hasMatch()) {
            backendStr = gfxBackendEnv;
        }
    }
    envExports << QString("export MACOBLOX_GRAPHICS_BACKEND=%1").arg(shellSingleQuote(backendStr));
    envExports << QString("export MACOBLOX_CPU_THREADS=%1").arg(m_cpuThreads);
    if (m_gpuVendor == "nvidia") {
        envExports << "export __NV_PRIME_RENDER_OFFLOAD=1";
        envExports << "export __GLX_VENDOR_LIBRARY_NAME=nvidia";
        envExports << "export __VK_LAYER_NV_optimus=NVIDIA_only";
    } else if (m_gpuVendor == "amd") {
        if (hasMultipleGpus()) {
            envExports << "export DRI_PRIME=1";
        }
        if (m_cpuThreads >= 6) {
            envExports << "export mesa_glthread=true";
        }
        envExports << "export RADV_PERFTEST=aco";
    } else if (m_gpuVendor == "intel") {
        if (m_cpuThreads >= 6) {
            envExports << "export mesa_glthread=true";
        }
    }

    if (backendStr == "vulkan") {
        envExports << "export MESA_LOADER_DRIVER_OVERRIDE=zink";
        envExports << "export GALLIUM_DRIVER=zink";
    }

    QString execCmd = "exec ./RobloxPlayer";
    if (!targetUri.trimmed().isEmpty()) {
        execCmd += " " + shellSingleQuote(targetUri.trimmed());
    }

    return QString("mkdir -p /private/tmp/Roblox/cache && cd %1 && %2 && %3")
        .arg(shellSingleQuote(appExecutableDir), envExports.join(" && "), execCmd);
}

QStringList DarlingController::buildFlatpakArgs(const QString &targetUri) const {
    return {"shell", "/bin/bash", "-c", buildDarlingLaunchScript(targetUri)};
}

bool DarlingController::hasMultipleGpus() const {
    int count = 0;
    QDir drmDir("/sys/class/drm");
    for (const QString &entry : drmDir.entryList({"card[0-9]*"}, QDir::Dirs)) {
        if (!entry.contains('-')) {
            count++;
        }
    }
    return count > 1;
}

bool DarlingController::isTelemetryDomain(const QString &domain) {
    const QString d = domain.trimmed().toLower();
    return d == "pulsar.roblox.com" ||
           d.endsWith(".pulsar.roblox.com") ||
           d == "ecsv2.roblox.com" ||
           d.endsWith(".ecsv2.roblox.com") ||
           d == "ephemeralcounters.api.roblox.com" ||
           d.endsWith(".ephemeralcounters.api.roblox.com");
}

QByteArray DarlingController::buildNxDomainResponse(const QByteArray &query, int questionEnd) {
    if (query.size() < 12 || questionEnd > query.size()) return {};
    QByteArray resp;
    resp.append(query.left(2)); // Transaction ID
    resp.append(static_cast<char>(0x81));
    resp.append(static_cast<char>(0x83)); // Response + NXDOMAIN (RCODE = 3)
    resp.append(query.mid(4, 2));         // QDCOUNT
    resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ANCOUNT = 0
    resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // NSCOUNT = 0
    resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ARCOUNT = 0
    resp.append(query.mid(12, questionEnd - 12));
    return resp;
}

QByteArray DarlingController::buildHostDnsResponse(const QByteArray &query) {
    if (query.size() < 12) return {};

    // Extract QNAME starting at offset 12
    int offset = 12;
    QString domain;
    while (offset < query.size()) {
        const quint8 len = static_cast<quint8>(query[offset++]);
        if (len == 0) break;
        if (offset + len > query.size()) return {};
        if (!domain.isEmpty()) domain.append('.');
        domain.append(QString::fromUtf8(query.mid(offset, len)));
        offset += len;
    }
    if (offset + 4 > query.size()) return {};
    const int questionEnd = offset + 4;
    const quint16 qtype = (static_cast<quint8>(query[offset]) << 8) | static_cast<quint8>(query[offset + 1]);

    if (isTelemetryDomain(domain)) {
        return buildNxDomainResponse(query, questionEnd);
    }

    if (qtype != 1) {
        // Return standard NODATA (NOERROR with ANCOUNT=0) for non-A queries (e.g. AAAA = 28)
        QByteArray resp;
        resp.append(query.left(2)); // Transaction ID
        resp.append(static_cast<char>(0x81));
        resp.append(static_cast<char>(0x80)); // Response + No error
        resp.append(query.mid(4, 2));         // QDCOUNT
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ANCOUNT = 0
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // NSCOUNT = 0
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ARCOUNT = 0
        resp.append(query.mid(12, questionEnd - 12));
        return resp;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    const int rc = ::getaddrinfo(domain.toUtf8().constData(), nullptr, &hints, &res);

    QByteArray resp;
    resp.append(query.left(2)); // Transaction ID
    if (rc != 0 || !res) {
        resp.append(static_cast<char>(0x81));
        resp.append(static_cast<char>(0x83)); // Response + NXDOMAIN
        resp.append(query.mid(4, 2));         // QDCOUNT
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ANCOUNT = 0
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // NSCOUNT = 0
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ARCOUNT = 0
        resp.append(query.mid(12, questionEnd - 12));
        if (res) ::freeaddrinfo(res);
        return resp;
    }

    QList<quint32> addrs;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        if (p->ai_family == AF_INET && p->ai_addr) {
            auto *sin = reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
            addrs.append(sin->sin_addr.s_addr);
        }
    }
    ::freeaddrinfo(res);
    if (addrs.isEmpty()) {
        resp.append(static_cast<char>(0x81));
        resp.append(static_cast<char>(0x83)); // NXDOMAIN
        resp.append(query.mid(4, 2));         // QDCOUNT
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ANCOUNT = 0
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // NSCOUNT = 0
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ARCOUNT = 0
        resp.append(query.mid(12, questionEnd - 12));
        return resp;
    }

    resp.append(static_cast<char>(0x81));
    resp.append(static_cast<char>(0x80)); // Response + No error
    resp.append(query.mid(4, 2));         // QDCOUNT
    const quint16 ancount = static_cast<quint16>(addrs.size());
    resp.append(static_cast<char>((ancount >> 8) & 0xFF));
    resp.append(static_cast<char>(ancount & 0xFF));
    resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // NSCOUNT = 0
    resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00)); // ARCOUNT = 0
    resp.append(query.mid(12, questionEnd - 12));

    for (quint32 ip : addrs) {
        resp.append(static_cast<char>(0xC0));
        resp.append(static_cast<char>(0x0C)); // Pointer to QNAME at offset 12
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x01)); // Type A
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x01)); // Class IN
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x00));
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x3C)); // TTL = 60s
        resp.append(static_cast<char>(0x00)); resp.append(static_cast<char>(0x04)); // Len = 4
        resp.append(reinterpret_cast<const char *>(&ip), 4);
    }
    return resp;
}

void DarlingController::startDnsForwarder() {
    stopDnsForwarder();
    const AppSettings &s = m_settingsMgr->settings();

    switch (s.dnsProvider) {
    case 1: m_dnsUpstreamIp = "9.9.9.9"; break;
    case 2: m_dnsUpstreamIp = "1.1.1.1"; break;
    case 3: m_dnsUpstreamIp = "8.8.8.8"; break;
    case 4: m_dnsUpstreamIp = s.dnsCustom.trimmed().isEmpty() ? "9.9.9.9" : s.dnsCustom.trimmed(); break;
    default: m_dnsUpstreamIp = QString{}; break; // 0 = System DNS via host resolver
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

            // Fast-path: immediately return NXDOMAIN for telemetry domains (e.g. pulsar.roblox.com)
            int offset = 12;
            QString domain;
            while (offset < query.size()) {
                const quint8 len = static_cast<quint8>(query[offset++]);
                if (len == 0) break;
                if (offset + len > query.size()) break;
                if (!domain.isEmpty()) domain.append('.');
                domain.append(QString::fromUtf8(query.mid(offset, len)));
                offset += len;
            }
            if (offset + 4 <= query.size() && isTelemetryDomain(domain)) {
                const QByteArray payload = buildNxDomainResponse(query, offset + 4);
                m_dnsUdp->writeDatagram(payload, senderHost, senderPort);
                continue;
            }

            {
                std::lock_guard<std::mutex> lk(m_dnsCacheMutex);
                auto it = m_dnsCache.find(domain);
                if (it != m_dnsCache.end()) {
                    QByteArray cached = it.value();
                    if (cached.size() >= 2) {
                        cached[0] = query[0];
                        cached[1] = query[1];
                        m_dnsUdp->writeDatagram(cached, senderHost, senderPort);
                        continue;
                    }
                }
            }

            const QString upstream = m_dnsUpstreamIp;
            QPointer<QUdpSocket> udpGuard(m_dnsUdp);
            (void)QtConcurrent::run([this, query, domain, upstream, senderHost, senderPort, udpGuard]() {
                auto cacheAndSend = [this, domain, udpGuard, senderHost, senderPort](const QByteArray &payload) {
                    if (payload.isEmpty() || !udpGuard) return;
                    if (!domain.isEmpty()) {
                        std::lock_guard<std::mutex> lk(m_dnsCacheMutex);
                        if (m_dnsCache.size() > 500) m_dnsCache.clear();
                        m_dnsCache[domain] = payload;
                    }
                    QMetaObject::invokeMethod(udpGuard, [udpGuard, payload, senderHost, senderPort]() {
                        if (udpGuard) udpGuard->writeDatagram(payload, senderHost, senderPort);
                    }, Qt::QueuedConnection);
                };

                // If System DNS or if DoT fails, resolve locally via host glibc
                if (upstream.isEmpty()) {
                    cacheAndSend(buildHostDnsResponse(query));
                    return;
                }

                // Attempt DNS-over-TLS with fast fallback to host DNS (120ms max)
                QSslSocket tls;
                tls.setPeerVerifyMode(QSslSocket::VerifyNone);
                tls.connectToHostEncrypted(upstream, 853);
                if (tls.waitForEncrypted(120)) {
                    QByteArray framed;
                    const quint16 qlen = static_cast<quint16>(query.size());
                    framed.append(static_cast<char>((qlen >> 8) & 0xFF));
                    framed.append(static_cast<char>(qlen & 0xFF));
                    framed.append(query);
                    tls.write(framed);
                    if (tls.waitForBytesWritten(120)) {
                        QByteArray respBuf;
                        while (respBuf.size() < 2 && tls.waitForReadyRead(120)) {
                            respBuf.append(tls.readAll());
                        }
                        if (respBuf.size() >= 2) {
                            const int rlen = (static_cast<unsigned char>(respBuf[0]) << 8) |
                                             static_cast<unsigned char>(respBuf[1]);
                            while (respBuf.size() < 2 + rlen && tls.waitForReadyRead(120)) {
                                respBuf.append(tls.readAll());
                            }
                            if (respBuf.size() >= 2 + rlen) {
                                cacheAndSend(respBuf.mid(2, rlen));
                                return;
                            }
                        }
                    }
                }

                // DoT failed or timed out: fall back immediately to host resolver
                cacheAndSend(buildHostDnsResponse(query));
            });
        }
    });
}

void DarlingController::stopDnsForwarder() {
    m_dnsPort = 0;
    {
        std::lock_guard<std::mutex> lk(m_dnsCacheMutex);
        m_dnsCache.clear();
    }
    if (m_dnsUdp) {
        m_dnsUdp->close();
        m_dnsUdp->deleteLater();
        m_dnsUdp = nullptr;
    }
}

bool DarlingController::isRobloxProcessAlive() {
    const uid_t myUid = ::getuid();
    QDir procDir("/proc");
    for (const QString &pidStr : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const pid_t pid = static_cast<pid_t>(pidStr.toInt(&ok));
        if (!ok || pid <= 1 || pid == ::getpid()) continue;

        const QString procPath = QString("/proc/%1").arg(pid);
        if (QFileInfo(procPath).ownerId() != myUid) continue;

        // Skip zombie / defunct processes
        QFile statFile(procPath + "/stat");
        if (statFile.open(QIODevice::ReadOnly)) {
            const QByteArray statData = statFile.readAll();
            int closeParen = statData.lastIndexOf(')');
            if (closeParen > 0 && closeParen + 2 < statData.size()) {
                char state = statData.at(closeParen + 2);
                if (state == 'Z') continue;
            }
        }

        QFile commFile(procPath + "/comm");
        QString comm;
        if (commFile.open(QIODevice::ReadOnly)) {
            comm = QString::fromUtf8(commFile.readAll()).trimmed();
        }
        if (comm == "bwrap" || comm == "flatpak" || comm == "sh" ||
            comm == "bash" || comm == "darling" || comm == "darlingserver" ||
            comm == "RobloxCrashHandler") {
            continue;
        }

        if (comm == "RobloxPlayer") {
            return true;
        }

        if (comm == "Main" || comm == "mldr") {
            QFile cmdFile(procPath + "/cmdline");
            if (cmdFile.open(QIODevice::ReadOnly)) {
                const QByteArray cmdline = cmdFile.readAll();
                const QList<QByteArray> args = cmdline.split('\0');
                if (!args.isEmpty()) {
                    const QByteArray argv0 = args.first();
                    if (argv0.endsWith("RobloxPlayer") && !argv0.contains("RobloxCrashHandler")) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
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
        bool isOurBinary = (comm == "RobloxPlayer" || comm == "RobloxCrashHandler" ||
                            comm == "darlingserver" || comm == "darling" ||
                            comm == "launchd" || comm == "mldr" || comm == "Main");
        if (!isOurBinary && (comm == "bwrap" || comm == "sh" || comm == "bash")) {
            // Orphaned MacOBlox Flatpak sandboxes keep a dead darlingserver's prefix busy.
            QFile cmdFile(procPath + "/cmdline");
            if (cmdFile.open(QIODevice::ReadOnly)) {
                const QByteArray cmd = cmdFile.readAll();
                isOurBinary = cmd.contains("darling") || cmd.contains("MacOBlox");
            }
        }
        if (!isOurBinary) continue;

        ::kill(pid, SIGKILL);
    }
    QStringList candidatePrefixes;
    if (!myPrefix.isEmpty()) candidatePrefixes << myPrefix;
    candidatePrefixes << (QDir::homePath() + "/.darling");
    const QString xdgData = qEnvironmentVariable("XDG_DATA_HOME");
    const QString baseData = !xdgData.trimmed().isEmpty() ? xdgData.trimmed() : (QDir::homePath() + "/.local/share");
    candidatePrefixes << (baseData + "/darling");
    candidatePrefixes << (QDir::homePath() + "/.var/app/xyz.narez.MacOBlox/data/darling");

    auto isPidAlive = [](pid_t p) -> bool {
        if (p <= 1) return false;
        return (::kill(p, 0) == 0);
    };

    auto shouldRemovePidAndSock = [&](const QString &pidFilePath) -> bool {
        QFile pidFile(pidFilePath);
        if (!pidFile.exists()) return true;
        if (pidFile.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const pid_t p = static_cast<pid_t>(pidFile.readAll().trimmed().toInt(&ok));
            if (ok && isPidAlive(p)) {
                return false; // Process recorded in .init.pid is still running; keep it!
            }
        }
        return true;
    };

    for (const QString &pfx : candidatePrefixes) {
        if (!pfx.isEmpty() && QDir(pfx).exists()) {
            const QString pidPath = pfx + "/.init.pid";
            if (shouldRemovePidAndSock(pidPath)) {
                QFile::remove(pidPath);
                QFile::remove(pfx + "/.darlingserver.sock");
            }
        }
    }
    // Clean up any stale darling sockets in /tmp owned by current user
    QDir tmpDir("/tmp");
    for (const QString &entry : tmpDir.entryList({".darlingserver*", ".init.pid*"}, QDir::Files | QDir::System)) {
        const QString p = tmpDir.filePath(entry);
        if (QFileInfo(p).ownerId() == myUid) {
            if (entry.startsWith(".init.pid")) {
                if (shouldRemovePidAndSock(p)) {
                    QFile::remove(p);
                }
            } else {
                QFile::remove(p);
            }
        }
    }
}

void DarlingController::launchRoblox(const QString &customUri) {
    m_manualStop = false;
    m_inActivePlace = false;
    m_lastLaunchTime = QDateTime::currentDateTime();
    m_lastExitTime = QDateTime();
    m_lastExitCode = 0;
    m_lastCrashed = false;
    m_recentDarlingLog.clear();
    if (m_procMonitorTimer) {
        m_procMonitorTimer->stop();
        m_procMonitorTimer->deleteLater();
    }
    m_settingsMgr->saveAll();

    ensureRobloxExecPermissions(m_settingsMgr->robloxAppDir());
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

    const QString dprefix = m_settingsMgr->darlingPrefixDir();
    if (!dprefix.isEmpty() && QFile::exists("/etc/resolv.conf")) {
        const QString darlingResolv = dprefix + "/etc/resolv.conf";
        QFile::remove(darlingResolv);
        QFile::copy("/etc/resolv.conf", darlingResolv);
    }

    const QString targetUri = !customUri.isEmpty()
                                  ? customUri
                                  : SettingsManager::parsePlaceUri(m_settingsMgr->settings().lastPlaceInput);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Darling's CGL uses X11/XWayland; keep Qt6 native on Wayland while setting EGL_PLATFORM=x11 for Darling
    env.insert("EGL_PLATFORM", "x11");
    if (!dprefix.isEmpty()) {
        env.insert("DPREFIX", dprefix);
    }
    m_gameProc->setProcessEnvironment(env);

    const QString darlingBin = findDarlingBin().isEmpty() ? QString("darling") : findDarlingBin();
    const QStringList args = buildFlatpakArgs(targetUri);
    emit logGenerated("[🚀] Запуск MacOBlox (Darling): " + darlingBin + " " + args.join(' '));
    m_gameProc->start(darlingBin, args);
    m_gameProc->closeWriteChannel();
    if (m_launchWatchdog) {
        m_launchWatchdog->start(35000);
    }
}

void DarlingController::stopRobloxAsync() {
    m_manualStop = true;
    m_inActivePlace = false;
    if (m_launchWatchdog && m_launchWatchdog->isActive()) {
        m_launchWatchdog->stop();
    }
    if (m_procMonitorTimer) {
        m_procMonitorTimer->stop();
        m_procMonitorTimer->deleteLater();
    }
    stopDnsForwarder();

    (void)QtConcurrent::run([this]() {
        clearStaleDarlingProcesses();
        QMetaObject::invokeMethod(this, [this]() {
            if (m_gameProc && m_gameProc->state() != QProcess::NotRunning) {
                m_gameProc->terminate();
                QTimer::singleShot(1200, m_gameProc, [this]() {
                    if (m_gameProc && m_gameProc->state() != QProcess::NotRunning) {
                        m_gameProc->kill();
                    }
                });
            }
            emit logGenerated("[🛑] Сессия RobloxPlayer внутри Darling завершена.");
        }, Qt::QueuedConnection);
    });
}

void DarlingController::restartDarlingAsync() {
    stopDnsForwarder();
    emit logGenerated("[🔄] Перезапуск контейнера Darling...");
    (void)QtConcurrent::run([this]() {
        clearStaleDarlingProcesses();
        QMetaObject::invokeMethod(this, [this]() {
            emit logGenerated("[✓] Контейнер Darling перезапущен и очищен.");
            emit toastRequested("🔄 Контейнер Darling перезапущен");
        }, Qt::QueuedConnection);
    });
}

void DarlingController::saveCurrentSessionToSlotAsync() {
    const QString cleanActiveId = QFileInfo(m_settingsMgr->activeAccountId()).fileName();
    if (cleanActiveId.isEmpty() || cleanActiveId.contains("..")) return;
    const QString macobloxDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString slotDir =
        m_settingsMgr->accountsDir() + "/" + cleanActiveId + "/MacOBlox";
    (void)QtConcurrent::run([macobloxDir, slotDir]() {
        if (QDir(macobloxDir).exists()) {
            QDir(slotDir).removeRecursively();
            copyDirectoryContent(macobloxDir, slotDir);
        }
    });
}

void DarlingController::switchAccountSlotAsync(const QString &targetAccountId) {
    stopRobloxAsync();
    const QString cleanTargetId = QFileInfo(targetAccountId).fileName();
    const QString cleanActiveId = QFileInfo(m_settingsMgr->activeAccountId()).fileName();
    if (cleanTargetId.isEmpty() || cleanTargetId.contains("..") ||
        cleanActiveId.isEmpty() || cleanActiveId.contains("..")) {
        return;
    }
    const QString macobloxDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString oldSlot =
        m_settingsMgr->accountsDir() + "/" + cleanActiveId + "/MacOBlox";
    const QString newSlot =
        m_settingsMgr->accountsDir() + "/" + cleanTargetId + "/MacOBlox";

    m_settingsMgr->setActiveAccountId(cleanTargetId);
    m_settingsMgr->saveAll();

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, cleanTargetId]() {
        watcher->deleteLater();
        emit accountSwitchCompleted(cleanTargetId);
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

    connect(m_wizardProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_wizardBusy = false;
        const QString msg = "Не удалось запустить сборку шимов (ошибка запуска процесса)";
        m_recentErrors.append(msg);
        emit wizardFinished(false, msg);
    });

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
        (void)QtConcurrent::run([this]() {
            prepareDarlingPrefixSync();
            QMetaObject::invokeMethod(this, &DarlingController::stepDownloadOrUpdateRoblox, Qt::QueuedConnection);
        });
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
            ensureRobloxExecPermissions(m_settingsMgr->robloxAppDir());
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

                        ensureRobloxExecPermissions(appDir);

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
    if (m_launchWatchdog && m_launchWatchdog->isActive()) {
        m_launchWatchdog->stop();
    }
    const QStringList &markers = m_settingsMgr->settings().leaveMarkers;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const QString &rawLine : chunk.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.contains("ROBLOSECURITY", Qt::CaseInsensitive) ||
            line.contains("Bearer ", Qt::CaseInsensitive)) {
            continue;
        }
        m_recentDarlingLog.append(line);
        while (m_recentDarlingLog.size() > 200) {
            m_recentDarlingLog.removeFirst();
        }
        const bool isBenignNoise =
            line.contains("RobloxPlayerInstaller") ||
            line.contains("RobloxCrashHandler") ||
            line.contains("ClientCrashpadHandler") ||
            line.contains("Evaluating deferred inferred crashes") ||
            line.contains("GL_KHR_no_error") ||
            line.contains("fromData:error:") ||
            line.contains("mi_option_") ||
            line.contains("TombstoneCache") ||
            line.contains("HeightmapDebugPS") ||
            line.contains("failed to parse uniforms in a shader") ||
            line.contains("mac_util.cc:141") ||
            line.contains("semaphore_timedwait failed") ||
            line.contains("semaphore_signal failed") ||
            line.contains("failed to increase FD rlimit") ||
            line.contains("DefaultUnified") ||
            line.contains("pulsar.roblox.com") ||
            line.contains("setStartupThrottle") ||
            line.contains("preRenderJob is null") ||
            line.contains("SignalRCoreError") ||
            line.contains("LuauForge") ||
            line.contains("multiget-playability-status") ||
            line.contains("Wrap-deformer") ||
            line.contains("fetching meshes error");

        if (!isBenignNoise &&
            (line.contains("error", Qt::CaseInsensitive) ||
             line.contains("fail", Qt::CaseInsensitive) ||
             line.contains("crash", Qt::CaseInsensitive) ||
             line.contains("stall", Qt::CaseInsensitive))) {
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
