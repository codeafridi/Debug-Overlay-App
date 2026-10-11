#include "XdotoolFocusProvider.h"

#include "HostCommand.h"
#include "LinuxProcess.h"

#include <QProcess>

namespace {
constexpr int kStartTimeoutMs = 200;
constexpr int kFinishTimeoutMs = 500;
} // namespace

XdotoolFocusProvider::XdotoolFocusProvider(QString command)
    : m_command(std::move(command)) {
}

QString XdotoolFocusProvider::name() const {
    return QStringLiteral("xdotool");
}

int XdotoolFocusProvider::focusedPid() const {
    QProcess process;
    const HostCommand command =
        hostcmd::wrap(m_command, {QStringLiteral("getwindowfocus"),
                                  QStringLiteral("getwindowpid")});
    process.start(command.program, command.arguments);

    if (!process.waitForStarted(kStartTimeoutMs)) {
        return -1;
    }

    if (!process.waitForFinished(kFinishTimeoutMs)) {
        process.kill();
        process.waitForFinished(100);
        return -1;
    }

    if (process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        return -1;
    }

    bool ok = false;
    const int pid = process.readAllStandardOutput().trimmed().toInt(&ok);
    if (!ok || pid <= 0) {
        return -1;
    }

    return linuxproc::exists(pid) ? pid : -1;
}
