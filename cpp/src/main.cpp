#include "HudWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QSocketNotifier>
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
    hud.applyPlaceholderValues(QStringLiteral("example-app"),
                               QStringLiteral("--"),
                               QStringLiteral("--"),
                               QStringLiteral("--"),
                               QStringLiteral("IDLE"));
    hud.show();

    return app.exec();
}
