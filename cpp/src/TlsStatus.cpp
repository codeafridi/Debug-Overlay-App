#include "TlsStatus.h"

int daysUntil(const QDateTime &when, const QDateTime &now) {
    if (!when.isValid()) {
        return 0;
    }
    return static_cast<int>(now.date().daysTo(when.date()));
}

QString tlsSeverity(int daysLeft) {
    if (daysLeft < 0) {
        return QStringLiteral("expired");
    }
    if (daysLeft <= 7) {
        return QStringLiteral("critical");
    }
    if (daysLeft <= 30) {
        return QStringLiteral("warning");
    }
    return QStringLiteral("ok");
}

QString tlsSummaryText(const TlsSnapshot &snapshot) {
    if (!snapshot.available) {
        return QStringLiteral("tls: no endpoints configured");
    }
    int expiring = 0;
    int worst = 100000;
    for (const TlsResult &result : snapshot.results) {
        if (!result.ok) {
            ++expiring;
            continue;
        }
        worst = qMin(worst, result.daysLeft);
        if (result.daysLeft <= 30) {
            ++expiring;
        }
    }
    return QStringLiteral("%1 endpoint(s)  nearest expiry %2d  %3 warning(s)")
        .arg(snapshot.results.size())
        .arg(worst == 100000 ? 0 : worst)
        .arg(expiring);
}

QString tlsDetailText(const TlsSnapshot &snapshot) {
    if (snapshot.results.isEmpty()) {
        return QStringLiteral("--");
    }
    QString text;
    for (const TlsResult &result : snapshot.results) {
        if (!result.ok) {
            text += QStringLiteral("%1:%2  DOWN  %3\n")
                        .arg(result.host)
                        .arg(result.port)
                        .arg(result.error);
            continue;
        }
        text += QStringLiteral("%1:%2  %3  %4d left  %5  %6\n")
                    .arg(result.host)
                    .arg(result.port)
                    .arg(tlsSeverity(result.daysLeft).toUpper().leftJustified(8, ' '))
                    .arg(result.daysLeft)
                    .arg(result.protocol, result.cipher);
    }
    return text.trimmed();
}
