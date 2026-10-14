#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct ProbeResult {
    QString url;
    bool success = false;
    int status = 0;
    double totalMs = 0.0;
    double dnsMs = 0.0;
    double connectMs = 0.0;
    double tlsMs = 0.0;
    double processingMs = 0.0;
    double transferMs = 0.0;
    QString error;
    qint64 atMs = 0;
};

struct SyntheticSnapshot {
    bool available = false;
    QList<ProbeResult> results;
    int upCount = 0;
    int downCount = 0;
    qint64 sampledAtMs = 0;
};

// Parses one `curl -w` line:
//   "<http_code> <time_namelookup> <time_connect> <time_appconnect>
//    <time_starttransfer> <time_total>"
// (times in seconds). Decomposes into blackbox_exporter-style phases (ms).
bool parseCurlWriteOut(const QByteArray &out, int &status, double &totalMs,
                       double &dnsMs, double &connectMs, double &tlsMs,
                       double &processingMs, double &transferMs);

QString syntheticSummaryText(const SyntheticSnapshot &snapshot);
QString syntheticDetailText(const SyntheticSnapshot &snapshot);
