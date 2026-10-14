#include "TraceStatus.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QString anyValue(const QJsonValue &value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        static const char *const keys[] = {"stringValue", "intValue",
                                           "doubleValue", "boolValue"};
        for (const char *key : keys) {
            if (obj.contains(QLatin1String(key))) {
                return anyValue(obj.value(QLatin1String(key)));
            }
        }
    }
    return QString();
}

QHash<QString, QString> readAttributes(const QJsonArray &attributes) {
    QHash<QString, QString> out;
    for (const QJsonValue &value : attributes) {
        const QJsonObject attr = value.toObject();
        out.insert(attr.value(QStringLiteral("key")).toString(),
                   anyValue(attr.value(QStringLiteral("value"))));
    }
    return out;
}

QString kindName(int kind) {
    switch (kind) {
    case 2:
        return QStringLiteral("server");
    case 3:
        return QStringLiteral("client");
    case 4:
        return QStringLiteral("producer");
    case 5:
        return QStringLiteral("consumer");
    case 1:
        return QStringLiteral("internal");
    default:
        return QStringLiteral("unspecified");
    }
}

double percentile(QList<double> values, double p) {
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int idx = static_cast<int>(std::ceil(p * values.size())) - 1;
    const int clamped = std::max(0, std::min(idx, static_cast<int>(values.size()) - 1));
    return values.at(clamped);
}

} // namespace

QList<Span> parseOtlpTraces(const QByteArray &json, bool *ok) {
    QList<Span> spans;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (ok) {
            *ok = false;
        }
        return spans;
    }
    const QJsonArray resourceSpans =
        doc.object().value(QStringLiteral("resourceSpans")).toArray();
    for (const QJsonValue &rsValue : resourceSpans) {
        const QJsonObject rs = rsValue.toObject();
        const QHash<QString, QString> resourceAttrs = readAttributes(
            rs.value(QStringLiteral("resource"))
                .toObject()
                .value(QStringLiteral("attributes"))
                .toArray());
        const QString service = resourceAttrs.value(QStringLiteral("service.name"),
                                                    QStringLiteral("unknown"));
        const QJsonArray scopeSpans =
            rs.value(QStringLiteral("scopeSpans")).toArray();
        for (const QJsonValue &ssValue : scopeSpans) {
            const QJsonArray innerSpans =
                ssValue.toObject().value(QStringLiteral("spans")).toArray();
            for (const QJsonValue &spanValue : innerSpans) {
                const QJsonObject obj = spanValue.toObject();
                Span span;
                span.traceId =
                    obj.value(QStringLiteral("traceId")).toString();
                span.spanId = obj.value(QStringLiteral("spanId")).toString();
                span.parentSpanId =
                    obj.value(QStringLiteral("parentSpanId")).toString();
                span.name = obj.value(QStringLiteral("name")).toString();
                span.service = service;
                span.kind = kindName(
                    obj.value(QStringLiteral("kind")).toInt());
                span.startNs =
                    obj.value(QStringLiteral("startTimeUnixNano"))
                        .toString()
                        .toLongLong();
                span.endNs = obj.value(QStringLiteral("endTimeUnixNano"))
                                 .toString()
                                 .toLongLong();
                span.attributes = readAttributes(
                    obj.value(QStringLiteral("attributes")).toArray());
                const int statusCode = obj.value(QStringLiteral("status"))
                                           .toObject()
                                           .value(QStringLiteral("code"))
                                           .toInt();
                span.status = statusCode == 2   ? QStringLiteral("error")
                              : statusCode == 1 ? QStringLiteral("ok")
                                                : QStringLiteral("unset");
                spans.append(span);
            }
        }
    }
    if (ok) {
        *ok = true;
    }
    return spans;
}

TraceSnapshot summarizeTraces(const QList<Span> &spans) {
    TraceSnapshot snapshot;
    snapshot.available = true;
    snapshot.spanCount = spans.size();

    QHash<QString, QList<double>> serviceDurations;
    QHash<QString, int> serviceSpans;
    QHash<QString, int> serviceErrors;
    QHash<QString, int> edgeCounts;

    QHash<QString, QString> spanService; // spanId -> service
    for (const Span &span : spans) {
        spanService.insert(span.spanId, span.service);
        serviceSpans[span.service] += 1;
        serviceDurations[span.service].append(span.durationMs());
        if (span.status == QStringLiteral("error")) {
            serviceErrors[span.service] += 1;
            ++snapshot.errorCount;
        }
    }

    for (const Span &span : spans) {
        if (span.parentSpanId.isEmpty() || !spanService.contains(span.parentSpanId)) {
            continue;
        }
        const QString parentService = spanService.value(span.parentSpanId);
        if (parentService != span.service) {
            edgeCounts[parentService + QStringLiteral("\t") + span.service] += 1;
        }
    }

    for (auto it = serviceSpans.constBegin(); it != serviceSpans.constEnd(); ++it) {
        ServiceLatency latency;
        latency.service = it.key();
        latency.spans = it.value();
        latency.errors = serviceErrors.value(it.key());
        const QList<double> durations = serviceDurations.value(it.key());
        double sum = 0.0;
        for (double d : durations) {
            sum += d;
            latency.maxMs = std::max(latency.maxMs, d);
        }
        latency.avgMs = durations.isEmpty() ? 0.0 : sum / durations.size();
        latency.p95Ms = percentile(durations, 0.95);
        snapshot.services.append(latency);
    }
    std::sort(snapshot.services.begin(), snapshot.services.end(),
              [](const ServiceLatency &a, const ServiceLatency &b) {
                  return a.avgMs > b.avgMs;
              });

    for (auto it = edgeCounts.constBegin(); it != edgeCounts.constEnd(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('\t'));
        ServiceEdge edge;
        edge.from = parts.value(0);
        edge.to = parts.value(1);
        edge.count = it.value();
        snapshot.edges.append(edge);
    }
    std::sort(snapshot.edges.begin(), snapshot.edges.end(),
              [](const ServiceEdge &a, const ServiceEdge &b) {
                  return a.count > b.count;
              });

    QHash<QString, QList<Span>> byTrace;
    for (const Span &span : spans) {
        byTrace[span.traceId].append(span);
    }
    snapshot.traceCount = byTrace.size();
    for (auto it = byTrace.constBegin(); it != byTrace.constEnd(); ++it) {
        const QList<Span> &traceSpans = it.value();
        TraceSummary trace;
        trace.traceId = it.key();
        trace.spanCount = traceSpans.size();
        qint64 minStart = std::numeric_limits<qint64>::max();
        qint64 maxEnd = 0;
        for (const Span &span : traceSpans) {
            if (span.parentSpanId.isEmpty()) {
                trace.rootName = span.name;
                trace.rootService = span.service;
            }
            minStart = std::min(minStart, span.startNs);
            maxEnd = std::max(maxEnd, span.endNs);
            if (span.status == QStringLiteral("error")) {
                trace.hasError = true;
            }
        }
        if (trace.rootName.isEmpty() && !traceSpans.isEmpty()) {
            trace.rootName = traceSpans.first().name;
            trace.rootService = traceSpans.first().service;
        }
        trace.durationMs =
            maxEnd > minStart ? static_cast<double>(maxEnd - minStart) / 1e6 : 0.0;
        trace.startMs = minStart / 1000000;
        snapshot.recent.append(trace);
    }
    std::sort(snapshot.recent.begin(), snapshot.recent.end(),
              [](const TraceSummary &a, const TraceSummary &b) {
                  return a.startMs > b.startMs;
              });
    if (snapshot.recent.size() > 25) {
        snapshot.recent = snapshot.recent.mid(0, 25);
    }

    return snapshot;
}

QString traceSummaryText(const TraceSnapshot &snapshot) {
    if (!snapshot.available) {
        return QStringLiteral("traces: waiting for OTLP");
    }
    return QStringLiteral("%1 traces  %2 spans  %3 errors  %4 services")
        .arg(snapshot.traceCount)
        .arg(snapshot.spanCount)
        .arg(snapshot.errorCount)
        .arg(snapshot.services.size());
}

QString traceServiceMapText(const TraceSnapshot &snapshot) {
    if (snapshot.edges.isEmpty()) {
        return QStringLiteral("no cross-service calls yet");
    }
    QString text;
    for (const ServiceEdge &edge : snapshot.edges) {
        text += QStringLiteral("%1 -> %2   x%3\n")
                    .arg(edge.from, edge.to)
                    .arg(edge.count);
    }
    return text.trimmed();
}

QString traceServicesText(const TraceSnapshot &snapshot) {
    if (snapshot.services.isEmpty()) {
        return QStringLiteral("--");
    }
    QString text;
    for (const ServiceLatency &service : snapshot.services) {
        text += QStringLiteral("%1  %2 spans  avg %3ms  p95 %4ms  max %5ms  %6 err\n")
                    .arg(service.service.leftJustified(14, ' '))
                    .arg(service.spans)
                    .arg(QString::number(service.avgMs, 'f', 1),
                         QString::number(service.p95Ms, 'f', 1),
                         QString::number(service.maxMs, 'f', 1))
                    .arg(service.errors);
    }
    return text.trimmed();
}

QString traceRecentText(const TraceSnapshot &snapshot) {
    if (snapshot.recent.isEmpty()) {
        return QStringLiteral("--");
    }
    QString text;
    for (const TraceSummary &trace : snapshot.recent) {
        text += QStringLiteral("%1  %2  %3 spans  %4ms%5\n")
                    .arg(trace.hasError ? QStringLiteral("ERR")
                                        : QStringLiteral("ok "),
                         trace.rootService.leftJustified(14, ' '))
                    .arg(trace.spanCount)
                    .arg(QString::number(trace.durationMs, 'f', 1),
                         trace.rootName.isEmpty()
                             ? QString()
                             : QStringLiteral("  ") + trace.rootName);
    }
    return text.trimmed();
}
