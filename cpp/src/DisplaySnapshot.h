#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

struct ProcessRow {
    int pid = 0;
    int depth = 0;
    QString name;
    QString memory;
};

struct DisplaySnapshot {
    QString appName = QStringLiteral("--");
    QString pid = QStringLiteral("--");
    QString cpu = QStringLiteral("--");
    QString memory = QStringLiteral("--");
    QString status = QStringLiteral("IDLE");
    QString memDetail;
    QList<ProcessRow> processes;
    quint64 sequence = 0;
};
