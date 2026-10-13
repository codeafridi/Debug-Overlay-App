#include "ServicesStatus.h"

bool parseSystemctlUnits(const QByteArray &text, QList<UnitRow> &out) {
    const QList<QByteArray> lines = text.split('\n');
    bool any = false;

    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        QList<QByteArray> fields = trimmed.simplified().split(' ');
        // systemctl marks failed units with a leading U+25CF bullet.
        if (!fields.isEmpty()
            && fields.first() == QByteArrayLiteral("\xE2\x97\x8F")) {
            fields.removeFirst();
        }
        if (fields.size() < 4) {
            continue;
        }

        UnitRow row;
        row.name = QString::fromUtf8(fields.at(0));
        row.load = QString::fromUtf8(fields.at(1));
        row.active = QString::fromUtf8(fields.at(2));
        row.sub = QString::fromUtf8(fields.at(3));
        row.description = QString::fromUtf8(fields.mid(4).join(' '));
        if (!row.name.contains(QLatin1Char('.'))) {
            continue; // skip summary/footer lines such as "19 loaded units listed."
        }
        out.append(row);
        any = true;
    }

    return any;
}

int servicesFailedCount(const QList<UnitRow> &units) {
    int count = 0;
    for (const UnitRow &row : units) {
        if (row.active == QStringLiteral("failed")
            || row.sub == QStringLiteral("failed")) {
            ++count;
        }
    }
    return count;
}

int servicesRunningCount(const QList<UnitRow> &units) {
    int count = 0;
    for (const UnitRow &row : units) {
        if (row.active == QStringLiteral("active")
            && row.sub == QStringLiteral("running")) {
            ++count;
        }
    }
    return count;
}

QString servicesSummaryText(const ServicesSnapshot &snapshot) {
    if (!snapshot.available) {
        return snapshot.errorMessage.isEmpty()
                   ? QStringLiteral("services: --")
                   : QStringLiteral("services: %1").arg(snapshot.errorMessage);
    }
    return QStringLiteral("services: %1 running | %2 failed")
        .arg(snapshot.runningCount)
        .arg(snapshot.failedCount);
}

QString servicesDetailText(const ServicesSnapshot &snapshot) {
    QStringList lines;
    for (const UnitRow &row : snapshot.units) {
        if (row.active == QStringLiteral("failed")
            || row.sub == QStringLiteral("failed")) {
            lines.append(QStringLiteral("%1 [%2/%3]").arg(row.name, row.active,
                                                          row.sub));
        }
    }
    return lines.isEmpty() ? QStringLiteral("no issues") : lines.join('\n');
}
