#pragma once

#include <QString>
#include <QStringList>

struct HostCommand {
    QString program;
    QStringList arguments;
};

namespace hostcmd {

bool inFlatpak();

QString resolveHostProgram(const QString &name);

HostCommand wrap(const QString &program, const QStringList &arguments);

} // namespace hostcmd
