#include "ThreatStatus.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

ThreatEvent parseAuthEvent(const QString &line, qint64 atMs) {
    ThreatEvent event;
    event.atMs = atMs;

    static const QRegularExpression failedPassword(
        QStringLiteral(R"(Failed password for (?:invalid user )?(\S+) from (\S+) port)"));
    static const QRegularExpression invalidUser(
        QStringLiteral(R"(Invalid user (\S+) from (\S+))"));
    static const QRegularExpression accepted(
        QStringLiteral(R"(Accepted (?:password|publickey|keyboard-interactive/pam) for (\S+) from (\S+))"));
    static const QRegularExpression pamFailure(
        QStringLiteral(R"(authentication failure;.*rhost=(\S+))"));

    QRegularExpressionMatch match = failedPassword.match(line);
    if (match.hasMatch()) {
        event.logType = QStringLiteral("ssh_failed-auth");
        event.targetUser = match.captured(1);
        event.sourceIp = match.captured(2);
        return event;
    }
    match = invalidUser.match(line);
    if (match.hasMatch()) {
        event.logType = QStringLiteral("ssh_bad_user");
        event.targetUser = match.captured(1);
        event.sourceIp = match.captured(2);
        return event;
    }
    match = accepted.match(line);
    if (match.hasMatch()) {
        event.logType = QStringLiteral("ssh_success");
        event.targetUser = match.captured(1);
        event.sourceIp = match.captured(2);
        return event;
    }
    match = pamFailure.match(line);
    if (match.hasMatch()) {
        event.logType = QStringLiteral("pam_failed-auth");
        event.sourceIp = match.captured(1);
        return event;
    }
    return event; // empty logType => not a security event
}

const QList<ThreatScenario> &threatScenarios() {
    static const QList<ThreatScenario> scenarios = [] {
        QList<ThreatScenario> list;
        auto add = [&list](const QString &name, const QString &label,
                           const QString &severity, const QStringList &logTypes,
                           const QString &service, int capacity,
                           double leakspeed, bool distinctUser) {
            ThreatScenario s;
            s.name = name;
            s.label = label;
            s.severity = severity;
            s.logTypes = logTypes;
            s.service = service;
            s.capacity = capacity;
            s.leakspeedSeconds = leakspeed;
            s.distinctUser = distinctUser;
            list.append(s);
        };
        // Ported from the CrowdSec hub leaksy scenarios.
        add(QStringLiteral("crowdsecurity/ssh-bf"),
            QStringLiteral("SSH Bruteforce"), QStringLiteral("high"),
            {QStringLiteral("ssh_failed-auth")}, QStringLiteral("ssh"), 5, 10.0,
            false);
        add(QStringLiteral("crowdsecurity/ssh-bf_user-enum"),
            QStringLiteral("SSH User Enumeration"), QStringLiteral("medium"),
            {QStringLiteral("ssh_failed-auth"), QStringLiteral("ssh_bad_user")},
            QStringLiteral("ssh"), 5, 10.0, true);
        add(QStringLiteral("crowdsecurity/pam-bf"),
            QStringLiteral("PAM/Login Bruteforce"), QStringLiteral("high"),
            {QStringLiteral("pam_failed-auth")}, QStringLiteral("system"), 5,
            10.0, false);
        return list;
    }();
    return scenarios;
}

ThreatReport evaluateThreats(const QList<ThreatEvent> &events) {
    ThreatReport report;
    report.available = true;
    report.events = events.size();

    for (const ThreatScenario &scenario : threatScenarios()) {
        // Gather matching events, grouped by (source IP [, user]).
        QHash<QString, QList<ThreatEvent>> buckets;
        for (const ThreatEvent &event : events) {
            if (!scenario.logTypes.contains(event.logType)) {
                continue;
            }
            if (event.sourceIp.isEmpty()) {
                continue;
            }
            const QString key =
                scenario.distinctUser
                    ? event.sourceIp + QLatin1Char('\t') + event.targetUser
                    : event.sourceIp;
            buckets[key].append(event);
        }

        for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
            QList<ThreatEvent> group = it.value();
            std::sort(group.begin(), group.end(),
                      [](const ThreatEvent &a, const ThreatEvent &b) {
                          return a.atMs < b.atMs;
                      });
            const double leakRate =
                scenario.capacity / std::max(1.0, scenario.leakspeedSeconds);
            double bucket = 0.0;
            qint64 last = group.first().atMs;
            for (const ThreatEvent &event : group) {
                const double elapsedSeconds =
                    static_cast<double>(event.atMs - last) / 1000.0;
                bucket = std::max(0.0, bucket - elapsedSeconds * leakRate);
                bucket += 1.0;
                last = event.atMs;
                if (bucket >= scenario.capacity) {
                    ThreatOverflow overflow;
                    overflow.scenario = scenario.name;
                    overflow.label = scenario.label;
                    overflow.severity = scenario.severity;
                    overflow.sourceIp = event.sourceIp;
                    overflow.count = static_cast<int>(bucket);
                    overflow.atMs = event.atMs;
                    report.findings.append(overflow);
                    ++report.overflows;
                    bucket = 0.0; // blackhole/reset after a decision
                    break;
                }
            }
        }
    }

    std::sort(report.findings.begin(), report.findings.end(),
              [](const ThreatOverflow &a, const ThreatOverflow &b) {
                  return a.atMs > b.atMs;
              });
    return report;
}

QString threatSummaryText(const ThreatReport &report) {
    if (!report.available) {
        return QStringLiteral("threats: unavailable");
    }
    return QStringLiteral("%1 overflow(s) from %2 security event(s)")
        .arg(report.overflows)
        .arg(report.events);
}

QString threatDetailText(const ThreatReport &report) {
    if (report.findings.isEmpty()) {
        return QStringLiteral("no brute-force / enumeration detected");
    }
    QString text;
    for (const ThreatOverflow &finding : report.findings) {
        text += QStringLiteral("%1  %2  from %3  (%4 hits)\n")
                    .arg(finding.severity.toUpper().leftJustified(8, ' '),
                         finding.label)
                    .arg(finding.sourceIp)
                    .arg(finding.count);
    }
    return text.trimmed();
}
