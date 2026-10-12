#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

struct ProcessRow {
    int pid = 0;
    int depth = 0;
    int threads = 0;
    int fds = -1;
    QString name;
    QString memory;
};

struct ConnectionRow {
    QString proto;
    QString local;
    QString remote;
    QString state;
};

struct DisplaySnapshot {
    QString appName = QStringLiteral("--");
    QString pid = QStringLiteral("--");
    QString cpu = QStringLiteral("--");
    QString memory = QStringLiteral("--");
    QString status = QStringLiteral("IDLE");
    unsigned long long rssKb = 0;
    unsigned long long vmsKb = 0;
    unsigned long long swapKb = 0;
    unsigned long long sharedKb = 0;
    QList<ProcessRow> processes;
    QList<ConnectionRow> connections;
    quint64 sequence = 0;
};
