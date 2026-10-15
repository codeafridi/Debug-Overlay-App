#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct DbmQuery {
    int pid = 0;
    QString state;
    QString wait;
    double elapsedSec = 0.0;
};

struct DbmSnapshot {
    bool available = false;
    QString error;
    QString database;
    int connections = 0;
    double cacheHitPercent = 0.0;
    quint64 xactCommit = 0;
    quint64 xactRollback = 0;
    quint64 deadlocks = 0;
    unsigned long long sizeBytes = 0;
    double blkReadTimeMs = 0.0;
    double blkWriteTimeMs = 0.0;
    double commitsPerSec = 0.0;
    QList<QPair<QString, int>> states;
    QList<DbmQuery> activeQueries;
};

// Parse a pg_stat_database row (column order fixed by the query) into the
// counters + instantaneous cache-hit ratio.
bool parseDatabaseStat(const QStringList &row, DbmSnapshot &out);
void parseActivityStates(const QList<QStringList> &rows, DbmSnapshot &out);
void parseActiveQueries(const QList<QStringList> &rows, DbmSnapshot &out);

QString dbmSummaryText(const DbmSnapshot &snapshot);
QString dbmDetailText(const DbmSnapshot &snapshot);
