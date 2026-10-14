#include "LogParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

#include <algorithm>

namespace {

QString severityField(const QHash<QString, QString> &fields) {
    static const QStringList keys = {QStringLiteral("level"),
                                     QStringLiteral("severity")};
    for (const QString &key : keys) {
        if (fields.contains(key)) {
            return fields.value(key).toLower();
        }
    }
    return QString();
}

QString codeField(const QHash<QString, QString> &fields) {
    static const QStringList keys = {QStringLiteral("code"),
                                     QStringLiteral("response_code"),
                                     QStringLiteral("status"),
                                     QStringLiteral("upstream_status")};
    for (const QString &key : keys) {
        if (fields.contains(key)) {
            const QString value = fields.value(key).trimmed();
            if (!value.isEmpty() && value != QLatin1String("-")) {
                return value;
            }
        }
    }
    return QString();
}

QString pathField(const QHash<QString, QString> &fields) {
    static const QStringList keys = {QStringLiteral("path"),
                                     QStringLiteral("uri"),
                                     QStringLiteral("url")};
    for (const QString &key : keys) {
        if (fields.contains(key) && !fields.value(key).isEmpty()) {
            return fields.value(key);
        }
    }
    return QString();
}

bool latencyField(const QHash<QString, QString> &fields, double &ms) {
    static const QStringList keys = {
        QStringLiteral("duration"),  QStringLiteral("request_time"),
        QStringLiteral("response_time"), QStringLiteral("upstream_response_time"),
        QStringLiteral("total_time"), QStringLiteral("ms"),
        QStringLiteral("latency")};
    for (const QString &key : keys) {
        if (!fields.contains(key)) {
            continue;
        }
        const QString value = fields.value(key).trimmed();
        if (value.isEmpty() || value == QLatin1String("-")) {
            continue;
        }
        bool ok = false;
        const double parsed = value.toDouble(&ok);
        if (!ok) {
            continue;
        }
        // Heuristic: values > 1000 are milliseconds; smaller are seconds.
        ms = parsed > 1000.0 ? parsed : parsed * 1000.0;
        return true;
    }
    return false;
}

void bump(QHash<QString, int> &counts, const QString &key) {
    if (!key.isEmpty()) {
        counts[key] += 1;
    }
}

QList<QPair<QString, int>> sortedCounts(const QHash<QString, int> &counts,
                                        int limit) {
    QList<QPair<QString, int>> out;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        out.append(qMakePair(it.key(), it.value()));
    }
    std::sort(out.begin(), out.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  if (a.second != b.second) {
                      return a.second > b.second;
                  }
                  return a.first < b.first;
              });
    if (out.size() > limit) {
        out = out.mid(0, limit);
    }
    return out;
}

} // namespace

const QList<LogParserDef> &logParsers() {
    static const QList<LogParserDef> parsers = [] {
        QList<LogParserDef> list;
        auto add = [&list](const QString &name, LogParserDef::Kind kind,
                           const QString &pattern) {
            LogParserDef def;
            def.name = name;
            def.kind = kind;
            def.pattern = pattern;
            list.append(def);
        };

        // Ported verbatim from Fluent Bit etc/fluent-bit/parsers.conf.
        add(QStringLiteral("json"), LogParserDef::Json, QString());
        add(QStringLiteral("logfmt"), LogParserDef::Logfmt, QString());
        add(QStringLiteral("docker"), LogParserDef::Json, QString());
        add(QStringLiteral("docker-daemon"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(time="(?<time>[^ ]*)" level=(?<level>[^ ]*) msg="(?<msg>[^ ].*)")RE"));
        add(QStringLiteral("syslog-rfc5424"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^\<(?<pri>[0-9]{1,5})\>1 (?<time>[^ ]+) (?<host>[^ ]+) (?<ident>[^ ]+) (?<pid>[-0-9]+) (?<msgid>[^ ]+) (?<extradata>(\[(.*?)\]|-)) (?<message>.+)$)RE"));
        add(QStringLiteral("syslog-rfc3164"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^\<(?<pri>[0-9]+)\>(?<time>[^ ]* {1,2}[^ ]* [^ ]*) (?<host>[^ ]*) (?<ident>[a-zA-Z0-9_\/\.\-]*)(?:\[(?<pid>[0-9]+)\])?(?:[^\:]*\:)? *(?<message>.*)$)RE"));
        add(QStringLiteral("cri"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^(?<time>[^ ]+) (?<stream>stdout|stderr) (?<logtag>[^ ]*) (?<message>.*)$)RE"));
        add(QStringLiteral("nginx"), LogParserDef::Regex,
            QStringLiteral("^(?<remote>[^ ]*) (?<host>[^ ]*) (?<user>[^ ]*) "
                           "\\[(?<time>[^\\]]*)\\] \"(?<method>\\S+)(?: "
                           "+(?<path>[^\\\"]*?)(?: +\\S*)?)?\" (?<code>[^ ]*) "
                           "(?<size>[^ ]*)(?: \"(?<referer>[^\\\"]*)\" "
                           "\"(?<agent>[^\\\"]*)\")"));
        add(QStringLiteral("apache2"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^(?<host>[^ ]*) [^ ]* (?<user>[^ ]*) \[(?<time>[^\]]*)\] "(?<method>\S+)(?: +(?<path>[^ ]*) +\S*)?" (?<code>[^ ]*) (?<size>[^ ]*)(?: "(?<referer>[^\"]*)" "(?<agent>.*)")?$)RE"));
        add(QStringLiteral("apache"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^(?<host>[^ ]*) [^ ]* (?<user>[^ ]*) \[(?<time>[^\]]*)\] "(?<method>\S+)(?: +(?<path>[^\"]*?)(?: +\S*)?)?" (?<code>[^ ]*) (?<size>[^ ]*)(?: "(?<referer>[^\"]*)" "(?<agent>[^\"]*)")?$)RE"));
        add(QStringLiteral("apache_error"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^\[[^ ]* (?<time>[^\]]*)\] \[(?<level>[^\]]*)\](?: \[pid (?<pid>[^\]]*)\])?( \[client (?<client>[^\]]*)\])? (?<message>.*)$)RE"));
        add(QStringLiteral("envoy"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^\[(?<start_time>[^\]]*)\] "(?<method>\S+)(?: +(?<path>[^\"]*?)(?: +\S*)?)? (?<protocol>\S+)" (?<code>[^ ]*) (?<response_flags>[^ ]*) (?<bytes_received>[^ ]*) (?<bytes_sent>[^ ]*) (?<duration>[^ ]*) (?<x_envoy_upstream_service_time>[^ ]*) "(?<x_forwarded_for>[^ ]*)" "(?<user_agent>[^\"]*)" "(?<request_id>[^\"]*)" "(?<authority>[^ ]*)" "(?<upstream_host>[^ ]*)")RE"));
        add(QStringLiteral("mongodb"), LogParserDef::Regex,
            QStringLiteral(
                R"RE(^(?<time>[^ ]*)\s+(?<severity>\w)\s+(?<component>[^ ]+)\s+\[(?<context>[^\]]+)]\s+(?<message>.*?) *(?<ms>(\d+))?(:?ms)?$)RE"));
        add(QStringLiteral("kube-custom"), LogParserDef::Regex,
            QStringLiteral(
                R"RE((?<tag>[^.]+)?\.?(?<pod_name>[a-z0-9](?:[-a-z0-9]*[a-z0-9])?(?:\.[a-z0-9]([-a-z0-9]*[a-z0-9])?)*)_(?<namespace_name>[^_]+)_(?<container_name>.+)-(?<docker_id>[a-z0-9]{64})\.log$)RE"));
        return list;
    }();
    return parsers;
}

bool parseLogLine(const QString &line, const LogParserDef &def,
                  QHash<QString, QString> &out) {
    if (line.isEmpty()) {
        return false;
    }
    if (def.kind == LogParserDef::Json) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) {
            return false;
        }
        const QJsonObject obj = doc.object();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            out.insert(it.key(), it.value().toVariant().toString());
        }
        return !out.isEmpty();
    }
    if (def.kind == LogParserDef::Logfmt) {
        static const QRegularExpression pair(
            QStringLiteral(R"RE(([A-Za-z0-9_.-]+)=("[^"]*"|[^\s"]+))RE"));
        QRegularExpressionMatchIterator it = pair.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            QString value = match.captured(2);
            if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
                value = value.mid(1, value.size() - 2);
            }
            out.insert(match.captured(1), value);
        }
        return !out.isEmpty();
    }

    static QHash<QString, QRegularExpression> compiled;
    auto found = compiled.constFind(def.pattern);
    if (found == compiled.constEnd()) {
        found = compiled.insert(def.pattern, QRegularExpression(def.pattern));
    }
    const QRegularExpression &re = found.value();
    if (!re.isValid()) {
        return false;
    }
    const QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch()) {
        return false;
    }
    const QStringList names = re.namedCaptureGroups();
    for (const QString &name : names) {
        if (name.isEmpty()) {
            continue;
        }
        const QString value = match.captured(name);
        if (!value.isNull()) {
            out.insert(name, value);
        }
    }
    return !out.isEmpty();
}

LogFields parseLogLineAuto(const QString &line) {
    LogFields result;
    for (const LogParserDef &def : logParsers()) {
        QHash<QString, QString> fields;
        if (parseLogLine(line, def, fields)) {
            result.parser = def.name;
            result.fields = fields;
            result.matched = true;
            return result;
        }
    }
    return result;
}

LogFacets computeLogFacets(const QStringList &lines) {
    LogFacets facets;
    facets.total = lines.size();
    QHash<QString, int> parserCounts;
    QHash<QString, int> levels;
    QHash<QString, int> codes;
    QHash<QString, int> paths;
    double latencySum = 0.0;

    for (const QString &line : lines) {
        const LogFields parsed = parseLogLineAuto(line);
        if (!parsed.matched) {
            continue;
        }
        ++facets.parsed;
        parserCounts[parsed.parser] += 1;
        bump(levels, severityField(parsed.fields));
        bump(codes, codeField(parsed.fields));
        bump(paths, pathField(parsed.fields));
        double ms = 0.0;
        if (latencyField(parsed.fields, ms)) {
            latencySum += ms;
            ++facets.latencyCount;
        }
    }

    int best = 0;
    for (auto it = parserCounts.constBegin(); it != parserCounts.constEnd();
         ++it) {
        if (it.value() > best) {
            best = it.value();
            facets.format = it.key();
        }
    }
    facets.levels = sortedCounts(levels, 8);
    facets.codes = sortedCounts(codes, 8);
    facets.paths = sortedCounts(paths, 8);
    if (facets.latencyCount > 0) {
        facets.avgLatencyMs = latencySum / facets.latencyCount;
    }
    return facets;
}

QString logPipelineSummaryText(const LogFacets &facets) {
    if (facets.total == 0) {
        return QStringLiteral("pipeline: no lines");
    }
    QString text =
        QStringLiteral("format %1  parsed %2/%3")
            .arg(facets.format.isEmpty() ? QStringLiteral("raw") : facets.format)
            .arg(facets.parsed)
            .arg(facets.total);
    if (!facets.levels.isEmpty()) {
        text += QStringLiteral("  levels ");
        for (const QPair<QString, int> &lvl : facets.levels) {
            text += QStringLiteral("%1:%2 ").arg(lvl.first).arg(lvl.second);
        }
    }
    return text;
}

QString logPipelineDetailText(const LogFacets &facets) {
    if (facets.total == 0) {
        return QStringLiteral("no log lines to parse");
    }
    QStringList lines;
    if (!facets.levels.isEmpty()) {
        QString l;
        for (const QPair<QString, int> &e : facets.levels) {
            l += QStringLiteral("%1 %2  ").arg(e.first).arg(e.second);
        }
        lines.append(QStringLiteral("levels  ") + l.trimmed());
    }
    if (!facets.codes.isEmpty()) {
        QString c;
        for (const QPair<QString, int> &e : facets.codes) {
            c += QStringLiteral("%1 x%2  ").arg(e.first).arg(e.second);
        }
        lines.append(QStringLiteral("codes   ") + c.trimmed());
    }
    if (!facets.paths.isEmpty()) {
        QString p;
        for (const QPair<QString, int> &e : facets.paths) {
            p += QStringLiteral("%1 x%2  ").arg(e.first).arg(e.second);
        }
        lines.append(QStringLiteral("paths   ") + p.trimmed());
    }
    if (facets.latencyCount > 0) {
        lines.append(QStringLiteral("latency avg %1 ms (%2 samples)")
                         .arg(QString::number(facets.avgLatencyMs, 'f', 1))
                         .arg(facets.latencyCount));
    }
    if (lines.isEmpty()) {
        lines.append(QStringLiteral("parsed %1 lines, no facets").arg(facets.parsed));
    }
    return lines.join(QLatin1Char('\n'));
}
