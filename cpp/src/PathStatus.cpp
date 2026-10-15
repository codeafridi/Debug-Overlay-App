#include "PathStatus.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

double PathReport::worstLoss() const {
    double worst = 0.0;
    for (const PathHop &hop : hops) {
        worst = std::max(worst, hop.lossPercent);
    }
    return worst;
}

double PathReport::avgRttMs() const {
    double sum = 0.0;
    int count = 0;
    for (const PathHop &hop : hops) {
        if (hop.avgMs > 0.0) {
            sum += hop.avgMs;
            ++count;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

QList<PathReport> parseMtrJson(const QByteArray &json, bool *ok) {
    QList<PathReport> reports;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (ok) {
            *ok = false;
        }
        return reports;
    }
    const QJsonObject report = doc.object().value(QStringLiteral("report")).toObject();
    const QJsonObject mtr = report.value(QStringLiteral("mtr")).toObject();

    PathReport out;
    out.src = mtr.value(QStringLiteral("src")).toString();
    out.dst = mtr.value(QStringLiteral("dst")).toString();
    out.tests = mtr.value(QStringLiteral("tests")).toInt();
    out.ok = true;

    const QJsonArray hubs = report.value(QStringLiteral("hubs")).toArray();
    for (const QJsonValue &value : hubs) {
        const QJsonObject hub = value.toObject();
        PathHop hop;
        hop.index = hub.value(QStringLiteral("count")).toInt();
        hop.host = hub.value(QStringLiteral("host")).toString();
        hop.lossPercent = hub.value(QStringLiteral("Loss%")).toDouble();
        hop.sent = hub.value(QStringLiteral("Snt")).toInt();
        hop.lastMs = hub.value(QStringLiteral("Last")).toDouble();
        hop.avgMs = hub.value(QStringLiteral("Avg")).toDouble();
        hop.bestMs = hub.value(QStringLiteral("Best")).toDouble();
        hop.worstMs = hub.value(QStringLiteral("Wrst")).toDouble();
        hop.stdevMs = hub.value(QStringLiteral("StDev")).toDouble();
        out.hops.append(hop);
    }
    reports.append(out);
    if (ok) {
        *ok = true;
    }
    return reports;
}

QString pathSummaryText(const PathSnapshot &snapshot) {
    if (!snapshot.available) {
        return QStringLiteral("path: no targets configured");
    }
    if (snapshot.reports.isEmpty()) {
        return QStringLiteral("path: waiting for first probe");
    }
    return QStringLiteral("%1 target(s) traced").arg(snapshot.reports.size());
}

QString pathHopsText(const PathReport &report) {
    if (report.hops.isEmpty()) {
        return QStringLiteral("no hops (unreachable?)");
    }
    QString text;
    for (const PathHop &hop : report.hops) {
        text += QStringLiteral("%1  %2  loss %3%  avg %4  best %5  wrst %6  stdev %7\n")
                    .arg(QString::number(hop.index).rightJustified(2, ' '),
                         hop.host.leftJustified(28, ' '))
                    .arg(QString::number(hop.lossPercent, 'f', 1))
                    .arg(QString::number(hop.avgMs, 'f', 1).rightJustified(7, ' '),
                         QString::number(hop.bestMs, 'f', 1).rightJustified(7, ' '),
                         QString::number(hop.worstMs, 'f', 1).rightJustified(7, ' '),
                         QString::number(hop.stdevMs, 'f', 1).rightJustified(7, ' '));
    }
    return text.trimmed();
}

QString pathTargetsText(const PathSnapshot &snapshot) {
    if (snapshot.reports.isEmpty()) {
        return QStringLiteral("--");
    }
    QString text;
    for (const PathReport &report : snapshot.reports) {
        text += QStringLiteral("%1  %2 hops  worst loss %3%  avg hop %4ms%5\n")
                    .arg(report.dst.leftJustified(24, ' '))
                    .arg(report.hops.size())
                    .arg(QString::number(report.worstLoss(), 'f', 1),
                         QString::number(report.avgRttMs(), 'f', 1))
                    .arg(report.worstLoss() > 0.0 ? QStringLiteral("  (loss!)")
                                                  : QString());
    }
    return text.trimmed();
}
