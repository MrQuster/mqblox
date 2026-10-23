#include "ui/MainWindow.hpp"
#include "ui/Widgets.hpp"
#include <QApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QPalette>
#include <sys/resource.h>

static void maximizeFileDescriptorLimit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        if (rl.rlim_cur < rl.rlim_max) {
            rl.rlim_cur = rl.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rl);
        }
    }
}

static const QString IPC_SERVER_NAME = "mqblox_single_instance_ipc_v16";

static void applyGlobalDarkPalette(QApplication &app) {
    app.setStyle("Fusion");
    QPalette pal;
    QColor bgDark(3, 4, 8), bgCard(10, 14, 25), txtLight(248, 250, 252), accent(251, 191, 36);
    pal.setColor(QPalette::Window, bgDark);
    pal.setColor(QPalette::WindowText, txtLight);
    pal.setColor(QPalette::Base, bgDark);
    pal.setColor(QPalette::AlternateBase, bgCard);
    pal.setColor(QPalette::ToolTipBase, bgDark);
    pal.setColor(QPalette::ToolTipText, accent);
    pal.setColor(QPalette::Text, txtLight);
    pal.setColor(QPalette::Button, bgCard);
    pal.setColor(QPalette::ButtonText, txtLight);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, QColor(45, 33, 18));
    pal.setColor(QPalette::HighlightedText, accent);
    app.setPalette(pal);
}

int main(int argc, char *argv[]) {
    maximizeFileDescriptorLimit();
    QLoggingCategory::setFilterRules("qt.qpa.wayland.textinput=false\nqt.core.qobject.connect=false");

    QApplication app(argc, argv);
    app.setApplicationName("mqblox");
    app.setApplicationDisplayName("MqBlox");
    app.setDesktopFileName("mqblox");
    app.setWindowIcon(QIcon::fromTheme("mqblox", QIcon(":/mqblox.svg")));
    app.setQuitOnLastWindowClosed(false);

    QLocalSocket probe;
    probe.connectToServer(IPC_SERVER_NAME);
    if (probe.waitForConnected(150)) {
        probe.write("SHOW");
        probe.waitForBytesWritten(150);
        probe.disconnectFromServer();
        return 0;
    }

    QLocalServer::removeServer(IPC_SERVER_NAME);
    QLocalServer ipcServer;
    ipcServer.listen(IPC_SERVER_NAME);

    applyGlobalDarkPalette(app);
    mqblox::installLiquidGlassEngine(&app);

    QString uriArg = (argc > 1) ? QString::fromUtf8(argv[1]) : QString{};
    mqblox::MainWindow win(uriArg);
    win.show();

    QObject::connect(&ipcServer, &QLocalServer::newConnection, &win, [&ipcServer, &win]() {
        while (QLocalSocket *client = ipcServer.nextPendingConnection()) {
            client->deleteLater();
            win.showNormal();
            win.raise();
            win.activateWindow();
        }
    });

    return app.exec();
}
