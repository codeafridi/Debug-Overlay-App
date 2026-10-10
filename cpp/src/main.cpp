#include "HudWindow.h"

#include "GnomePidFileProvider.h"
#include "SamplerWorker.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMetaType>
#include <QSocketNotifier>
#include <QThread>
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

    qRegisterMetaType<DisplaySnapshot>("DisplaySnapshot");

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
    hud.setSnapshot(DisplaySnapshot{});

    GnomePidFileProvider provider;

    auto *thread = new QThread;
    auto *worker = new SamplerWorker(&provider);
    worker->moveToThread(thread);
    QObject::connect(thread, &QThread::started, worker, &SamplerWorker::start);
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    quint64 lastSequence = 0;
    QObject::connect(worker, &SamplerWorker::snapshotReady, &hud,
                     [&hud, &lastSequence](const DisplaySnapshot &snapshot) {
                         if (snapshot.sequence <= lastSequence) {
                             return;
                         }
                         lastSequence = snapshot.sequence;
                         hud.setSnapshot(snapshot);
                     });

    thread->start();

    const QString pidFilePath = GnomePidFileProvider::defaultPath();
    QString lastRaw;
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

    auto requestSample = [&worker]() {
        QMetaObject::invokeMethod(worker, "sampleNow", Qt::QueuedConnection);
    };

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
        requestSample();
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

    hud.show();

    const int rc = app.exec();

    thread->quit();
    thread->wait();
    delete thread;

    return rc;
}
