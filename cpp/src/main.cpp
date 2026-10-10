#include "HudWindow.h"

#include "FocusSampler.h"
#include "GnomePidFileProvider.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QTimer>
#include <QtGlobal>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int g_signalFd[2] = {-1, -1};

void writeSignalByte(int) {
    const char byte = 0;
    const ssize_t written = ::write(g_signalFd[0], &byte, sizeof(byte));
    static_cast<void>(written);
}

} // namespace

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        && !qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Debug Overlay"));

    struct sigaction ignoreAction;
    ignoreAction.sa_handler = SIG_IGN;
    sigemptyset(&ignoreAction.sa_mask);
    ignoreAction.sa_flags = 0;
    ::sigaction(SIGTSTP, &ignoreAction, nullptr);
    ::sigaction(SIGTTIN, &ignoreAction, nullptr);
    ::sigaction(SIGTTOU, &ignoreAction, nullptr);

    QSocketNotifier *signalNotifier = nullptr;
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFd) == 0) {
        signalNotifier =
            new QSocketNotifier(g_signalFd[1], QSocketNotifier::Read, &app);

        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app,
                         [&app]() {
                             char byte = 0;
                             const ssize_t got =
                                 ::read(g_signalFd[1], &byte, sizeof(byte));
                             static_cast<void>(got);
                             app.quit();
                         });

        struct sigaction action;
        action.sa_handler = writeSignalByte;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        ::sigaction(SIGINT, &action, nullptr);
        ::sigaction(SIGTERM, &action, nullptr);
        ::sigaction(SIGHUP, &action, nullptr);
    }

    HudWindow hud;
    DisplaySnapshot snapshot;
    snapshot.status = QStringLiteral("IDLE");
    hud.setSnapshot(snapshot);

    GnomePidFileProvider provider;
    FocusSampler sampler(&provider);
    QString lastRaw;

    auto publish = [&hud, &sampler]() {
        const FocusSample focus = sampler.sample();

        DisplaySnapshot snap;
        if (focus.available) {
            snap.appName = focus.appName;
            snap.pid = QString::number(focus.identity.pid);
            snap.status = QStringLiteral("IDLE");
        } else {
            snap.status = QStringLiteral("no-focus");
        }
        hud.setSnapshot(snap);
    };

    publish();

    const QString pidFilePath = GnomePidFileProvider::defaultPath();
    auto *watcher = new QFileSystemWatcher(&app);
    if (!pidFilePath.isEmpty()) {
        const QFileInfo info(pidFilePath);
        const QString dirPath = info.absolutePath();
        if (QFileInfo::exists(dirPath)) {
            watcher->addPath(dirPath);
        }
        if (QFileInfo::exists(pidFilePath)) {
            watcher->addPath(pidFilePath);
        }
    }

    auto refreshIfChanged = [&]() {
        QFile file(pidFilePath);
        QString raw;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            raw = QString::fromUtf8(file.readAll()).trimmed();
        }
        if (raw == lastRaw) {
            return;
        }
        lastRaw = raw;
        publish();
    };

    QObject::connect(watcher, &QFileSystemWatcher::directoryChanged, &app,
                     [&](const QString &) {
                         refreshIfChanged();
                         if (!pidFilePath.isEmpty()
                             && QFileInfo::exists(pidFilePath)
                             && !watcher->files().contains(pidFilePath)) {
                             watcher->addPath(pidFilePath);
                         }
                     });

    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, &app,
                     [&](const QString &) {
                         refreshIfChanged();
                         if (!pidFilePath.isEmpty()
                             && QFileInfo::exists(pidFilePath)
                             && !watcher->files().contains(pidFilePath)) {
                             watcher->addPath(pidFilePath);
                         }
                     });

    auto *fallbackTimer = new QTimer(&app);
    fallbackTimer->setInterval(1000);
    QObject::connect(fallbackTimer, &QTimer::timeout, &app, [&]() {
        lastRaw.clear();
        publish();
    });
    fallbackTimer->start();

    hud.show();

    return app.exec();
}
