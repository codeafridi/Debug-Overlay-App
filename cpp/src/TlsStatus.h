#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtGlobal>

struct TlsResult {
    QString host;
    int port = 443;
    bool ok = false;
    QString error;
    QString subject;
    QString issuer;
    QDateTime notBefore;
    QDateTime notAfter;
    int daysLeft = 0;
    QString protocol;
    QString cipher;
};

struct TlsSnapshot {
    bool available = false;
    QList<TlsResult> results;
    qint64 sampledAtMs = 0;
};

int daysUntil(const QDateTime &when, const QDateTime &now);
QString tlsSeverity(int daysLeft);
QString tlsSummaryText(const TlsSnapshot &snapshot);
QString tlsDetailText(const TlsSnapshot &snapshot);
