#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct StatsdSeries {
    QString name;
    QString type; // counter/gauge/timer/histogram/distribution/set
    QString tags;
    double last = 0.0;
    double sum = 0.0;
    double min = 0.0;
    double max = 0.0;
    double avg = 0.0;
    quint64 count = 0;
    quint64 unique = 0;
};

struct StatsdSnapshot {
    bool available = false;
    quint64 datagrams = 0;
    quint64 metrics = 0;
    quint64 events = 0;
    quint64 serviceChecks = 0;
    quint64 parseErrors = 0;
    QList<StatsdSeries> series; // sorted by name
    qint64 sampledAtMs = 0;
};

struct StatsdPoint {
    QString name;
    QString type;
    QList<double> values;
    double sampleRate = 1.0;
    QString tags;
};

// DogStatsD single-line decoders.
bool parseStatsdMetric(const QString &line, StatsdPoint &out);
QString statsdTypeName(const QString &code);

// Aggregates DogStatsD datagrams the way the Datadog Agent does.
class StatsdAggregator {
public:
    void ingest(const QByteArray &datagram);
    void clear();
    StatsdSnapshot snapshot() const;

private:
    QHash<QString, StatsdSeries> m_series;
    QHash<QString, QSet<qint64>> m_setValues;
    quint64 m_datagrams = 0;
    quint64 m_metrics = 0;
    quint64 m_events = 0;
    quint64 m_serviceChecks = 0;
    quint64 m_parseErrors = 0;
};

QString statsdSummaryText(const StatsdSnapshot &snapshot);
QString statsdDetailText(const StatsdSnapshot &snapshot);
