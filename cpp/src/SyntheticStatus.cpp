#include "SyntheticStatus.h"

#include <QStringList>

namespace {

double nonNegative(double value) {
    return value < 0.0 ? 0.0 : value;
}

} // namespace

bool parseCurlWriteOut(const QByteArray &out, int &status, double &totalMs,
                       double &dnsMs, double &connectMs, double &tlsMs,
                       double &processingMs, double &transferMs) {
    const QList<QByteArray> fields = out.simplified().split(' ');
    if (fields.size() < 6) {
        return false;
    }

    bool okCode = false;
    status = fields.at(0).toInt(&okCode);
    bool ok0 = false;
    bool ok1 = false;
    bool ok2 = false;
    bool ok3 = false;
    bool ok4 = false;
    const double namelookup = fields.at(1).toDouble(&ok0);
    const double connect = fields.at(2).toDouble(&ok1);
    const double appconnect = fields.at(3).toDouble(&ok2);
    const double starttransfer = fields.at(4).toDouble(&ok3);
    const double total = fields.at(5).toDouble(&ok4);
    if (!okCode || !ok0 || !ok1 || !ok2 || !ok3 || !ok4) {
        return false;
    }

    dnsMs = nonNegative(namelookup) * 1000.0;
    connectMs = nonNegative(connect - namelookup) * 1000.0;
    tlsMs = nonNegative(appconnect - connect) * 1000.0;
    processingMs = nonNegative(starttransfer - appconnect) * 1000.0;
    transferMs = nonNegative(total - starttransfer) * 1000.0;
    totalMs = nonNegative(total) * 1000.0;
    return true;
}

QString syntheticSummaryText(const SyntheticSnapshot &snapshot) {
    if (snapshot.results.isEmpty()) {
        return QStringLiteral("synthetic: no targets configured");
    }
    return QStringLiteral("synthetic: %1 up | %2 down")
        .arg(snapshot.upCount)
        .arg(snapshot.downCount);
}

QString syntheticDetailText(const SyntheticSnapshot &snapshot) {
    if (snapshot.results.isEmpty()) {
        return QStringLiteral("add targets in [synthetic] targets = ...");
    }
    QStringList lines;
    for (const ProbeResult &result : snapshot.results) {
        if (result.success) {
            lines.append(QStringLiteral("OK   %1  %2  %3 ms")
                             .arg(result.status)
                             .arg(result.url)
                             .arg(result.totalMs, 0, 'f', 0));
        } else {
            const QString reason = result.error.isEmpty()
                                       ? (result.status != 0
                                              ? QStringLiteral("HTTP %1")
                                                    .arg(result.status)
                                              : QStringLiteral("failed"))
                                       : result.error;
            lines.append(QStringLiteral("DOWN %1  %2")
                             .arg(result.url, reason));
        }
    }
    return lines.join(QLatin1Char('\n'));
}
