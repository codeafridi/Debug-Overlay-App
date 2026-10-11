#include "HostCommand.h"

#include <QProcessEnvironment>
#include <QStandardPaths>

namespace hostcmd {

bool inFlatpak() {
    return !QProcessEnvironment::systemEnvironment()
                .value(QStringLiteral("FLATPAK_ID"))
                .isEmpty();
}

QString resolveHostProgram(const QString &name) {
    if (inFlatpak()) {
        return name;
    }
    return QStandardPaths::findExecutable(name);
}

HostCommand wrap(const QString &program, const QStringList &arguments) {
    HostCommand command;
    if (inFlatpak() && !program.startsWith(QLatin1Char('/'))) {
        command.program = QStringLiteral("flatpak-spawn");
        command.arguments = QStringList{QStringLiteral("--host"), program};
        command.arguments += arguments;
    } else {
        command.program = program;
        command.arguments = arguments;
    }
    return command;
}

} // namespace hostcmd
