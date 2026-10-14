#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// One built-in log parser (name + format), ported from Fluent Bit's
// shipped parsers.conf (etc/fluent-bit/parsers.conf).
struct LogParserDef {
    QString name;
    QString pattern; // for Format regex
    enum Kind { Regex, Json, Logfmt } kind = Regex;
};

struct LogFields {
    QString parser;
    QHash<QString, QString> fields;
    bool matched = false;
};

struct LogFacets {
    QString format;
    int total = 0;
    int parsed = 0;
    QList<QPair<QString, int>> levels;
    QList<QPair<QString, int>> codes;
    QList<QPair<QString, int>> paths;
    double avgLatencyMs = 0.0;
    int latencyCount = 0;
};

const QList<LogParserDef> &logParsers();

bool parseLogLine(const QString &line, const LogParserDef &def,
                  QHash<QString, QString> &out);
LogFields parseLogLineAuto(const QString &line);
LogFacets computeLogFacets(const QStringList &lines);

QString logPipelineSummaryText(const LogFacets &facets);
QString logPipelineDetailText(const LogFacets &facets);
