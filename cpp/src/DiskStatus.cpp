#include "DiskStatus.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace {

bool isPartition(const QString &name) {
    static const QRegularExpression patterns[] = {
        QRegularExpression(QStringLiteral("^(sd[a-z]+|vd[a-z]+|hd[a-z]+)\\d+$")),
        QRegularExpression(QStringLiteral("^nvme\\d+n\\d+p\\d+$")),
        QRegularExpression(QStringLiteral("^mmcblk\\d+p\\d+$")),
        QRegularExpression(QStringLiteral("^md\\d+p\\d+$")),
    };
    for (const QRegularExpression &pattern : patterns) {
        if (pattern.match(name).hasMatch()) {
            return true;
        }
    }
    return false;
}

double delta(quint64 previous, quint64 current) {
    return current >= previous ? static_cast<double>(current - previous) : 0.0;
}

} // namespace

QList<DiskCounters> parseDiskstats(const QByteArray &data) {
    QList<DiskCounters> out;
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QString line = QString::fromLatin1(rawLine).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList parts =
            line.split(QRegularExpression(QStringLiteral("\\s+")),
                       Qt::SkipEmptyParts);
        if (parts.size() < 14) {
            continue;
        }
        DiskCounters counters;
        counters.name = parts.at(2);
        counters.reads = parts.at(3).toULongLong();
        counters.sectorsRead = parts.at(5).toULongLong();
        counters.msReading = parts.at(6).toULongLong();
        counters.writes = parts.at(7).toULongLong();
        counters.sectorsWritten = parts.at(9).toULongLong();
        counters.msWriting = parts.at(10).toULongLong();
        counters.iosInProgress = parts.at(11).toULongLong();
        counters.msDoingIo = parts.at(12).toULongLong();
        counters.weightedMsDoingIo = parts.at(13).toULongLong();
        out.append(counters);
    }
    return out;
}

IoPressure parsePressureIo(const QByteArray &data) {
    IoPressure pressure;
    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QString line = QString::fromLatin1(rawLine).trimmed();
        if (!line.startsWith(QStringLiteral("some"))
            && !line.startsWith(QStringLiteral("full"))) {
            continue;
        }
        const bool some = line.startsWith(QStringLiteral("some"));
        static const QRegularExpression field(
            QStringLiteral("(avg10|avg60|avg300)=([0-9.]+)"));
        QRegularExpressionMatchIterator it = field.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString key = match.captured(1);
            const double value = match.captured(2).toDouble();
            if (some && key == QStringLiteral("avg10")) {
                pressure.someAvg10 = value;
            } else if (some && key == QStringLiteral("avg60")) {
                pressure.someAvg60 = value;
            } else if (some && key == QStringLiteral("avg300")) {
                pressure.someAvg300 = value;
            } else if (!some && key == QStringLiteral("avg10")) {
                pressure.fullAvg10 = value;
            } else if (!some && key == QStringLiteral("avg60")) {
                pressure.fullAvg60 = value;
            } else if (!some && key == QStringLiteral("avg300")) {
                pressure.fullAvg300 = value;
            }
            pressure.available = true;
        }
    }
    return pressure;
}

bool diskIsPhysical(const QString &name) {
    if (name.isEmpty()) {
        return false;
    }
    static const QStringList skipPrefixes = {
        QStringLiteral("loop"), QStringLiteral("ram"), QStringLiteral("zram"),
        QStringLiteral("sr"),   QStringLiteral("fd"),   QStringLiteral("dm-"),
        QStringLiteral("md")};
    for (const QString &prefix : skipPrefixes) {
        if (name.startsWith(prefix) && !name.startsWith(QStringLiteral("md"))) {
            return false;
        }
    }
    return !isPartition(name);
}

QList<DiskRow> computeDiskRates(const QList<DiskCounters> &previous,
                                const QList<DiskCounters> &current,
                                double intervalMs) {
    QList<DiskRow> rows;
    if (intervalMs <= 0.0) {
        return rows;
    }
    const double seconds = intervalMs / 1000.0;
    const double millis = intervalMs;

    for (const DiskCounters &cur : current) {
        if (!diskIsPhysical(cur.name)) {
            continue;
        }
        DiskCounters prev;
        bool found = false;
        for (const DiskCounters &p : previous) {
            if (p.name == cur.name) {
                prev = p;
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }

        const double dReads = delta(prev.reads, cur.reads);
        const double dWrites = delta(prev.writes, cur.writes);
        const double dSectorsRead = delta(prev.sectorsRead, cur.sectorsRead);
        const double dSectorsWritten =
            delta(prev.sectorsWritten, cur.sectorsWritten);
        const double dMsReading = delta(prev.msReading, cur.msReading);
        const double dMsWriting = delta(prev.msWriting, cur.msWriting);
        const double dMsDoingIo = delta(prev.msDoingIo, cur.msDoingIo);
        const double dWeighted =
            delta(prev.weightedMsDoingIo, cur.weightedMsDoingIo);

        DiskRow row;
        row.name = cur.name;
        row.readPerSec = dReads / seconds;
        row.writePerSec = dWrites / seconds;
        row.readKbPerSec = dSectorsRead * kDiskSectorBytes / 1024.0 / seconds;
        row.writeKbPerSec = dSectorsWritten * kDiskSectorBytes / 1024.0 / seconds;
        row.readAwaitMs = dReads > 0.0 ? dMsReading / dReads : 0.0;
        row.writeAwaitMs = dWrites > 0.0 ? dMsWriting / dWrites : 0.0;
        row.aquSz = dWeighted / millis;
        row.utilPercent = dMsDoingIo / millis * 100.0;
        rows.append(row);
    }

    std::sort(rows.begin(), rows.end(), [](const DiskRow &a, const DiskRow &b) {
        if (a.utilPercent != b.utilPercent) {
            return a.utilPercent > b.utilPercent;
        }
        return a.name < b.name;
    });
    return rows;
}

QString diskSummaryText(const DiskSnapshot &snapshot) {
    if (!snapshot.available) {
        return QStringLiteral("disks: unavailable");
    }
    if (snapshot.warming) {
        return QStringLiteral("disks: warming up");
    }
    double totalRead = 0.0;
    double totalWrite = 0.0;
    double worstUtil = 0.0;
    QString worstName;
    for (const DiskRow &row : snapshot.disks) {
        totalRead += row.readKbPerSec;
        totalWrite += row.writeKbPerSec;
        if (row.utilPercent > worstUtil) {
            worstUtil = row.utilPercent;
            worstName = row.name;
        }
    }
    QString text = QStringLiteral("r %1/s  w %2/s  busiest %3 %4%")
                       .arg(QString::number(totalRead, 'f', 0),
                            QString::number(totalWrite, 'f', 0),
                            worstName.isEmpty() ? QStringLiteral("-")
                                                : worstName,
                            QString::number(worstUtil, 'f', 0));
    if (snapshot.pressure.available) {
        text += QStringLiteral("  io-pressure some %1%")
                    .arg(QString::number(snapshot.pressure.someAvg60, 'f', 1));
    }
    return text;
}

QString diskDetailText(const DiskSnapshot &snapshot) {
    if (!snapshot.available) {
        return QStringLiteral("disks: unavailable");
    }
    if (snapshot.warming) {
        return QStringLiteral("disks: warming up");
    }
    QStringList lines;
    for (const DiskRow &row : snapshot.disks) {
        lines.append(QStringLiteral("%1  r %2/s  w %3/s  rkB %4  wkB %5  "
                                    "await %6  util %7%  aqu %8")
                         .arg(row.name.leftJustified(10, ' '),
                              QString::number(row.readPerSec, 'f', 1)
                                  .rightJustified(9, ' '),
                              QString::number(row.writePerSec, 'f', 1)
                                  .rightJustified(9, ' '),
                              QString::number(row.readKbPerSec, 'f', 0)
                                  .rightJustified(7, ' '),
                              QString::number(row.writeKbPerSec, 'f', 0)
                                  .rightJustified(7, ' '),
                              QString::number(row.readAwaitMs, 'f', 1)
                                  .rightJustified(7, ' '),
                              QString::number(row.utilPercent, 'f', 1)
                                  .rightJustified(6, ' '),
                              QString::number(row.aquSz, 'f', 2)
                                  .rightJustified(6, ' ')));
    }
    if (lines.isEmpty()) {
        lines.append(QStringLiteral("no block devices"));
    }
    return lines.join('\n');
}
