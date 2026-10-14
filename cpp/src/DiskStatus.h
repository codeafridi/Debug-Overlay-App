#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

constexpr double kDiskSectorBytes = 512.0;

struct DiskCounters {
    QString name;
    quint64 reads = 0;
    quint64 sectorsRead = 0;
    quint64 msReading = 0;
    quint64 writes = 0;
    quint64 sectorsWritten = 0;
    quint64 msWriting = 0;
    quint64 iosInProgress = 0;
    quint64 msDoingIo = 0;
    quint64 weightedMsDoingIo = 0;
};

struct DiskRow {
    QString name;
    double readPerSec = 0.0;
    double writePerSec = 0.0;
    double readKbPerSec = 0.0;
    double writeKbPerSec = 0.0;
    double readAwaitMs = 0.0;
    double writeAwaitMs = 0.0;
    double aquSz = 0.0;
    double utilPercent = 0.0;
};

struct IoPressure {
    bool available = false;
    double someAvg10 = 0.0;
    double someAvg60 = 0.0;
    double someAvg300 = 0.0;
    double fullAvg10 = 0.0;
    double fullAvg60 = 0.0;
    double fullAvg300 = 0.0;
};

struct DiskSnapshot {
    QList<DiskRow> disks;
    IoPressure pressure;
    double intervalMs = 0.0;
    bool available = false;
    bool warming = false;
    qint64 sampledAtMs = 0;
};

QList<DiskCounters> parseDiskstats(const QByteArray &data);
IoPressure parsePressureIo(const QByteArray &data);
bool diskIsPhysical(const QString &name);
QList<DiskRow> computeDiskRates(const QList<DiskCounters> &previous,
                                const QList<DiskCounters> &current,
                                double intervalMs);
QString diskSummaryText(const DiskSnapshot &snapshot);
QString diskDetailText(const DiskSnapshot &snapshot);
