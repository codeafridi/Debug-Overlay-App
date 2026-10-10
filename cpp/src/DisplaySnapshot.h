#pragma once

#include <QString>
#include <QtGlobal>

struct DisplaySnapshot {
    QString appName = QStringLiteral("--");
    QString pid = QStringLiteral("--");
    QString cpu = QStringLiteral("--");
    QString memory = QStringLiteral("--");
    QString status = QStringLiteral("IDLE");
    quint64 sequence = 0;
};
