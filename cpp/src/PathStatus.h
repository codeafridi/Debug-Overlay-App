#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct PathHop {
    int index = 0;
    QString host;
    double lossPercent = 0.0;
    int sent = 0;
    double lastMs = 0.0;
    double avgMs = 0.0;
    double bestMs = 0.0;
    double worstMs = 0.0;
    double stdevMs = 0.0;
};

struct PathReport {
    QString src;
    QString dst;
    int tests = 0;
    bool ok = false;
    QList<PathHop> hops;

    double worstLoss() const;
    double avgRttMs() const;
};

struct PathSnapshot {
    bool available = false;
    QList<PathReport> reports;
    qint64 sampledAtMs = 0;
};

// Decode `mtr --report --json` output (report.mtr + report.hubs[]).
QList<PathReport> parseMtrJson(const QByteArray &json, bool *ok = nullptr);
QString pathSummaryText(const PathSnapshot &snapshot);
QString pathHopsText(const PathReport &report);
QString pathTargetsText(const PathSnapshot &snapshot);
