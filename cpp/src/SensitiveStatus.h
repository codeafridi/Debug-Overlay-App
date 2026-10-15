#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

struct SensitiveRule {
    QString id;
    QString description;
    QString pattern;
};

struct SensitiveFinding {
    QString id;
    QString sample; // redacted
};

struct SensitiveReport {
    bool available = false;
    int scannedLines = 0;
    int findings = 0;
    QList<QPair<QString, int>> byRule;      // rule id -> count
    QList<QPair<QString, QString>> samples; // rule id -> redacted sample
};

// Secret/PII detector rules ported from gitleaks' config/gitleaks.toml.
const QList<SensitiveRule> &sensitiveRules();
QString redactSecret(const QString &value);
QList<SensitiveFinding> scanSensitiveLine(const QString &line);
SensitiveReport scanSensitiveLines(const QStringList &lines);
QString sensitiveSummaryText(const SensitiveReport &report);
QString sensitiveDetailText(const SensitiveReport &report);
