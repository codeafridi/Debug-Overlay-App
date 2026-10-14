#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QtGlobal>

struct Span {
    QString traceId;
    QString spanId;
    QString parentSpanId;
    QString name;
    QString service;
    QString kind;
    QString status; // ok / error / unset
    qint64 startNs = 0;
    qint64 endNs = 0;
    QHash<QString, QString> attributes;

    double durationMs() const {
        return endNs > startNs ? static_cast<double>(endNs - startNs) / 1e6 : 0.0;
    }
};

struct ServiceLatency {
    QString service;
    int spans = 0;
    int errors = 0;
    double avgMs = 0.0;
    double maxMs = 0.0;
    double p95Ms = 0.0;
};

struct ServiceEdge {
    QString from;
    QString to;
    int count = 0;
};

struct TraceSummary {
    QString traceId;
    QString rootName;
    QString rootService;
    double durationMs = 0.0;
    int spanCount = 0;
    bool hasError = false;
    qint64 startMs = 0;
};

struct TraceSnapshot {
    bool available = false;
    int spanCount = 0;
    int traceCount = 0;
    int errorCount = 0;
    QList<ServiceLatency> services;
    QList<ServiceEdge> edges;
    QList<TraceSummary> recent;
    qint64 sampledAtMs = 0;
};

// Decode OTLP/HTTP JSON traces (resourceSpans -> scopeSpans -> spans).
QList<Span> parseOtlpTraces(const QByteArray &json, bool *ok = nullptr);
TraceSnapshot summarizeTraces(const QList<Span> &spans);
QString traceSummaryText(const TraceSnapshot &snapshot);
QString traceServiceMapText(const TraceSnapshot &snapshot);
QString traceServicesText(const TraceSnapshot &snapshot);
QString traceRecentText(const TraceSnapshot &snapshot);
