#include "JournalStatus.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

QString decodeMessage(const QJsonValue &value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isArray()) {
        QByteArray bytes;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &item : array) {
            bytes.append(static_cast<char>(item.toInt()));
        }
        return QString::fromUtf8(bytes);
    }
    return QString();
}

} // namespace

bool parseJournalJson(const QByteArray &text, QList<JournalEntry> &out) {
    const QList<QByteArray> lines = text.split('\n');
    bool any = false;

    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty() || !trimmed.startsWith('{')) {
            continue;
        }

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(trimmed, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();

        JournalEntry entry;
        entry.timestampUsec =
            object.value(QStringLiteral("__REALTIME_TIMESTAMP")).toString().toLongLong();
        entry.priority =
            object.value(QStringLiteral("PRIORITY")).toString().toInt();
        entry.pid = object.value(QStringLiteral("_PID")).toString().toInt();
        entry.comm = object.value(QStringLiteral("_COMM")).toString();
        if (entry.comm.isEmpty()) {
            entry.comm = object.value(QStringLiteral("SYSLOG_IDENTIFIER")).toString();
        }
        entry.message = decodeMessage(object.value(QStringLiteral("MESSAGE")));
        out.append(entry);
        any = true;
    }

    return any;
}

bool journalEntryIsError(const JournalEntry &entry) {
    return entry.priority >= 0 && entry.priority <= 3;
}

int journalErrorCount(const QList<JournalEntry> &entries) {
    int count = 0;
    for (const JournalEntry &entry : entries) {
        if (journalEntryIsError(entry)) {
            ++count;
        }
    }
    return count;
}

QString journalSummaryText(const JournalSnapshot &snapshot) {
    if (!snapshot.available) {
        return snapshot.errorMessage.isEmpty()
                   ? QStringLiteral("logs: --")
                   : QStringLiteral("logs: %1").arg(snapshot.errorMessage);
    }
    return QStringLiteral("journald: %1 entries | %2 errors")
        .arg(snapshot.entries.size())
        .arg(snapshot.errorCount);
}

QString journalLogsText(const JournalSnapshot &snapshot, int maxLines) {
    QStringList lines;
    const int start = qMax(0, snapshot.entries.size() - maxLines);
    for (int i = start; i < snapshot.entries.size(); ++i) {
        const JournalEntry &entry = snapshot.entries.at(i);
        const QString time =
            QDateTime::fromMSecsSinceEpoch(entry.timestampUsec / 1000)
                .toString(QStringLiteral("HH:mm:ss"));
        lines.append(QStringLiteral("%1  %2: %3")
                         .arg(time, entry.comm, entry.message));
    }
    return lines.isEmpty() ? QStringLiteral("no log entries") : lines.join('\n');
}

QString journalErrorsText(const JournalSnapshot &snapshot) {
    QStringList lines;
    for (const JournalEntry &entry : snapshot.entries) {
        if (!journalEntryIsError(entry)) {
            continue;
        }
        const QString time =
            QDateTime::fromMSecsSinceEpoch(entry.timestampUsec / 1000)
                .toString(QStringLiteral("HH:mm:ss"));
        lines.append(QStringLiteral("%1  %2: %3").arg(time, entry.comm,
                                                      entry.message));
        if (lines.size() >= 60) {
            break;
        }
    }
    return lines.isEmpty() ? QStringLiteral("no errors") : lines.join('\n');
}
