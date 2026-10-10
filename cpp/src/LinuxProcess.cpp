#include "LinuxProcess.h"

#include <QFile>
#include <QFileInfo>

namespace linuxproc {

namespace {

QString procPath(int pid, const char *leaf) {
    return QStringLiteral("/proc/%1/%2").arg(pid).arg(QLatin1String(leaf));
}

} // namespace

bool exists(int pid) {
    if (pid <= 0) {
        return false;
    }
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

QString comm(int pid) {
    QFile file(procPath(pid, "comm"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

unsigned long long startTime(int pid) {
    QFile file(procPath(pid, "stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    const QString text = QString::fromUtf8(file.readAll());
    const int close = text.lastIndexOf(QLatin1Char(')'));
    if (close < 0) {
        return 0;
    }

    const QString tail = text.mid(close + 2).trimmed();
    const QStringList fields = tail.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    constexpr int startTimeIndex = 19;
    if (fields.size() <= startTimeIndex) {
        return 0;
    }

    bool ok = false;
    const unsigned long long value = fields.at(startTimeIndex).toULongLong(&ok);
    return ok ? value : 0;
}

ProcessIdentity identity(int pid) {
    ProcessIdentity id;
    if (!exists(pid)) {
        return id;
    }

    id.pid = pid;
    id.startTime = startTime(pid);
    if (id.startTime == 0) {
        id.pid = -1;
    }
    return id;
}

} // namespace linuxproc
