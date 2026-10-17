#pragma once

#include "AlertEngine.h"

#include <QList>
#include <QString>
#include <QStringList>

struct Incident {
    QString id;
    QString key;      // grouping key (alert rule id)
    QString title;
    QString severity;
    QString status;   // open / resolved
    qint64 openedAt = 0;
    qint64 lastSeen = 0;
    qint64 resolvedAt = 0;
    int count = 0;
    QStringList timeline;
};

// Alertmanager-style grouping: alerts sharing a group_by key are folded into a
// single incident; the incident resolves after group_interval of silence.
class IncidentTracker {
public:
    explicit IncidentTracker(qint64 resolveAfterMs = 60000,
                             int keepResolved = 30);

    void update(const QList<Alert> &active, qint64 nowMs);
    QList<Incident> incidents() const;
    void reset();

private:
    QList<Incident> m_incidents;
    qint64 m_resolveAfterMs;
    int m_keepResolved;
    int m_nextId = 1;
};

QString incidentsSummaryText(const QList<Incident> &incidents);
QString incidentsDetailText(const QList<Incident> &incidents);
