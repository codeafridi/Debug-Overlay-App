#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct JournalEntry {
    qint64 timestampUsec = 0;
    int priority = 6;
    int pid = 0;
    QString comm;
    QString message;
};

struct JournalSnapshot {
    bool available = false;
    QString errorMessage;
    QList<JournalEntry> entries;
    int errorCount = 0;
    qint64 sampledAtMs = 0;
};

// Parses newline-delimited JSON objects produced by `journalctl -o json`.
bool parseJournalJson(const QByteArray &text, QList<JournalEntry> &out);
bool journalEntryIsError(const JournalEntry &entry);
int journalErrorCount(const QList<JournalEntry> &entries);

QString journalSummaryText(const JournalSnapshot &snapshot);
QString journalLogsText(const JournalSnapshot &snapshot, int maxLines = 40);
QString journalErrorsText(const JournalSnapshot &snapshot);
