#include "StatsdStatus.h"

#include <algorithm>
#include <cstring>

QString statsdTypeName(const QString &code) {
    if (code == QLatin1String("c")) {
        return QStringLiteral("counter");
    }
    if (code == QLatin1String("g")) {
        return QStringLiteral("gauge");
    }
    if (code == QLatin1String("ms")) {
        return QStringLiteral("timer");
    }
    if (code == QLatin1String("h")) {
        return QStringLiteral("histogram");
    }
    if (code == QLatin1String("d")) {
        return QStringLiteral("distribution");
    }
    if (code == QLatin1String("s")) {
        return QStringLiteral("set");
    }
    return code;
}

bool parseStatsdMetric(const QString &line, StatsdPoint &out) {
    // <name>:<v1>[:<v2>...]|<type>|@<rate>|#<tags>
    const QStringList fields = line.split(QLatin1Char('|'));
    if (fields.size() < 2) {
        return false;
    }
    const QStringList head = fields.at(0).split(QLatin1Char(':'));
    if (head.size() < 2 || head.at(0).isEmpty()) {
        return false;
    }
    out.name = head.at(0);
    out.values.clear();
    for (int i = 1; i < head.size(); ++i) {
        bool ok = false;
        const double value = head.at(i).toDouble(&ok);
        if (ok) {
            out.values.append(value);
        }
    }
    if (out.values.isEmpty()) {
        return false;
    }
    out.type = statsdTypeName(fields.at(1));
    out.sampleRate = 1.0;
    out.tags.clear();
    for (int i = 2; i < fields.size(); ++i) {
        const QString field = fields.at(i);
        if (field.startsWith(QLatin1Char('@'))) {
            out.sampleRate = field.mid(1).toDouble();
            if (out.sampleRate <= 0.0) {
                out.sampleRate = 1.0;
            }
        } else if (field.startsWith(QLatin1Char('#'))) {
            out.tags = field.mid(1);
        }
    }
    return true;
}

void StatsdAggregator::ingest(const QByteArray &datagram) {
    ++m_datagrams;
    const QList<QByteArray> lines = datagram.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1String("_e{"))) {
            ++m_events;
            continue;
        }
        if (line.startsWith(QLatin1String("_sc|"))) {
            ++m_serviceChecks;
            continue;
        }
        StatsdPoint point;
        if (!parseStatsdMetric(line, point)) {
            ++m_parseErrors;
            continue;
        }
        ++m_metrics;
        const QString key = point.name + QLatin1Char('|') + point.tags;
        StatsdSeries &series = m_series[key];
        series.name = point.name;
        series.type = point.type;
        series.tags = point.tags;

        for (double value : point.values) {
            if (series.type == QStringLiteral("set")) {
                qint64 bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                m_setValues[key].insert(bits);
                series.unique = m_setValues.value(key).size();
                series.last = static_cast<double>(series.unique);
                continue;
            }
            series.last = value;
            if (series.type == QStringLiteral("counter")) {
                series.sum += value / point.sampleRate;
            } else {
                series.sum += value;
                series.min = series.count == 0 ? value
                                               : std::min(series.min, value);
                series.max = std::max(series.max, value);
                series.avg = series.sum / static_cast<double>(series.count + 1);
            }
            ++series.count;
        }
    }
}

void StatsdAggregator::clear() {
    m_series.clear();
    m_setValues.clear();
    m_datagrams = m_metrics = m_events = m_serviceChecks = m_parseErrors = 0;
}

StatsdSnapshot StatsdAggregator::snapshot() const {
    StatsdSnapshot snapshot;
    snapshot.available = true;
    snapshot.datagrams = m_datagrams;
    snapshot.metrics = m_metrics;
    snapshot.events = m_events;
    snapshot.serviceChecks = m_serviceChecks;
    snapshot.parseErrors = m_parseErrors;
    for (auto it = m_series.constBegin(); it != m_series.constEnd(); ++it) {
        snapshot.series.append(it.value());
    }
    std::sort(snapshot.series.begin(), snapshot.series.end(),
              [](const StatsdSeries &a, const StatsdSeries &b) {
                  if (a.name != b.name) {
                      return a.name < b.name;
                  }
                  return a.tags < b.tags;
              });
    return snapshot;
}

QString statsdSummaryText(const StatsdSnapshot &snapshot) {
    if (!snapshot.available) {
        return QStringLiteral("dogstatsd: not listening");
    }
    return QStringLiteral("%1 metrics  %2 series  %3 datagrams  %4 events  "
                          "%5 checks")
        .arg(snapshot.metrics)
        .arg(snapshot.series.size())
        .arg(snapshot.datagrams)
        .arg(snapshot.events)
        .arg(snapshot.serviceChecks);
}

QString statsdDetailText(const StatsdSnapshot &snapshot) {
    if (snapshot.series.isEmpty()) {
        return QStringLiteral("send DogStatsD metrics to 127.0.0.1:8125");
    }
    QString text;
    int shown = 0;
    for (const StatsdSeries &series : snapshot.series) {
        if (shown++ >= 40) {
            text += QStringLiteral("...\n");
            break;
        }
        QString value;
        if (series.type == QStringLiteral("counter")) {
            value = QStringLiteral("sum %1").arg(QString::number(series.sum, 'f', 2));
        } else if (series.type == QStringLiteral("set")) {
            value = QStringLiteral("unique %1").arg(series.unique);
        } else if (series.type == QStringLiteral("gauge")) {
            value = QStringLiteral("last %1").arg(QString::number(series.last, 'f', 2));
        } else {
            value = QStringLiteral("avg %1 max %2 n=%3")
                        .arg(QString::number(series.avg, 'f', 2),
                             QString::number(series.max, 'f', 2))
                        .arg(series.count);
        }
        text += QStringLiteral("%1 [%2] %3%4\n")
                    .arg(series.name.leftJustified(28, ' '), series.type, value,
                         series.tags.isEmpty()
                             ? QString()
                             : QStringLiteral("  #") + series.tags);
    }
    return text.trimmed();
}
