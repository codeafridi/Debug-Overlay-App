#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct ThreatEvent {
    qint64 atMs = 0;
    QString logType;   // ssh_failed-auth, ssh_bad_user, pam_failed-auth, ...
    QString sourceIp;
    QString targetUser;
};

struct ThreatScenario {
    QString name;
    QString label;
    QString severity;      // low/medium/high/critical
    QStringList logTypes;  // matched event log types
    QString service;
    int capacity = 5;
    double leakspeedSeconds = 10.0;
    bool distinctUser = false;
};

struct ThreatOverflow {
    QString scenario;
    QString label;
    QString severity;
    QString sourceIp;
    int count = 0;
    qint64 atMs = 0;
};

struct ThreatReport {
    bool available = false;
    int events = 0;
    int overflows = 0;
    QList<ThreatOverflow> findings;
};

// Parse a log line into a security event (CrowdSec-style log_type).
ThreatEvent parseAuthEvent(const QString &line, qint64 atMs);
// Leaky-bucket scenarios ported from the CrowdSec hub.
const QList<ThreatScenario> &threatScenarios();
ThreatReport evaluateThreats(const QList<ThreatEvent> &events);
QString threatSummaryText(const ThreatReport &report);
QString threatDetailText(const ThreatReport &report);
