#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct UnitRow {
    QString name;
    QString load;
    QString active;
    QString sub;
    QString description;
};

struct ServicesSnapshot {
    bool available = false;
    QString errorMessage;
    QList<UnitRow> units;
    int runningCount = 0;
    int failedCount = 0;
    qint64 sampledAtMs = 0;
};

bool parseSystemctlUnits(const QByteArray &text, QList<UnitRow> &out);
int servicesFailedCount(const QList<UnitRow> &units);
int servicesRunningCount(const QList<UnitRow> &units);

QString servicesSummaryText(const ServicesSnapshot &snapshot);
QString servicesDetailText(const ServicesSnapshot &snapshot);
