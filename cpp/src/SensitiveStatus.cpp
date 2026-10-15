#include "SensitiveStatus.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace {

bool luhnValid(const QString &digits) {
    int sum = 0;
    bool alternate = false;
    for (int i = digits.size() - 1; i >= 0; --i) {
        int n = digits.at(i).digitValue();
        if (n < 0) {
            return false;
        }
        if (alternate) {
            n *= 2;
            if (n > 9) {
                n -= 9;
            }
        }
        sum += n;
        alternate = !alternate;
    }
    return digits.size() >= 13 && digits.size() <= 19 && sum % 10 == 0;
}

} // namespace

const QList<SensitiveRule> &sensitiveRules() {
    static const QList<SensitiveRule> rules = [] {
        QList<SensitiveRule> list;
        auto add = [&list](const QString &id, const QString &desc,
                           const QString &pattern) {
            SensitiveRule rule;
            rule.id = id;
            rule.description = desc;
            rule.pattern = pattern;
            list.append(rule);
        };
        // Ported from gitleaks v8.30.1 config/gitleaks.toml (exact regexes).
        add(QStringLiteral("aws-access-token"),
            QStringLiteral("AWS Access Key ID"),
            QStringLiteral(R"((?i)\b((?:A3T[A-Z0-9]|AKIA|ASIA|ABIA|ACCA)[A-Z2-7]{16})\b)"));
        add(QStringLiteral("github-pat"), QStringLiteral("GitHub Personal Access Token"),
            QStringLiteral(R"(ghp_[0-9a-zA-Z]{36})"));
        add(QStringLiteral("github-oauth"), QStringLiteral("GitHub OAuth Access Token"),
            QStringLiteral(R"(gho_[0-9a-zA-Z]{36})"));
        add(QStringLiteral("github-refresh-token"),
            QStringLiteral("GitHub Refresh Token"),
            QStringLiteral(R"(ghr_[0-9a-zA-Z]{36})"));
        add(QStringLiteral("private-key"), QStringLiteral("Private Key"),
            QStringLiteral(R"((?i)-----BEGIN[ A-Z0-9_-]{0,100}PRIVATE KEY(?: BLOCK)?-----[\s\S-]{64,}?KEY(?: BLOCK)?-----)"));
        add(QStringLiteral("jwt"), QStringLiteral("JSON Web Token"),
            QStringLiteral(R"(\b(ey[a-zA-Z0-9]{17,}\.ey[a-zA-Z0-9\/\\_-]{17,}\.(?:[a-zA-Z0-9\/\\_-]{10,}={0,2})?)(?:[\x60'"\s;]|\\[nr]|$))"));
        add(QStringLiteral("stripe-access-token"),
            QStringLiteral("Stripe Access Token"),
            QStringLiteral(R"(\b((?:sk|rk)_(?:test|live|prod)_[a-zA-Z0-9]{10,99})(?:[\x60'"\s;]|\\[nr]|$))"));
        add(QStringLiteral("slack-webhook-url"),
            QStringLiteral("Slack Webhook URL"),
            QStringLiteral(R"((?:https?://)?hooks\.slack\.com/(?:services|workflows|triggers)/[A-Za-z0-9+/]{43,56})"));
        add(QStringLiteral("generic-api-key"), QStringLiteral("Generic API Key"),
            QStringLiteral(R"((?i)[\w.-]{0,50}?(?:access|auth|(?-i:[Aa]pi|API)|credential|creds|key|passw(?:or)?d|secret|token)(?:[ \t\w.-]{0,20})[\s'"]{0,3}(?:=|>|:{1,3}=|\|\||:|=>|\?=|,)[\x60'"\s=]{0,5}([\w.=-]{10,150}|[a-z0-9][a-z0-9+/]{11,}={0,3})(?:[\x60'"\s;]|\\[nr]|$))"));
        add(QStringLiteral("email"), QStringLiteral("Email Address"),
            QStringLiteral(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)"));
        return list;
    }();
    return rules;
}

QString redactSecret(const QString &value) {
    const int len = value.size();
    if (len <= 4) {
        return QString(len, QLatin1Char('*'));
    }
    if (len <= 12) {
        return value.left(2) + QString(len - 2, QLatin1Char('*'));
    }
    return value.left(4) + QStringLiteral("...") + value.right(2)
           + QStringLiteral(" (%1)").arg(len);
}

QList<SensitiveFinding> scanSensitiveLine(const QString &line) {
    QList<SensitiveFinding> findings;
    static QHash<QString, QRegularExpression> compiled;
    for (const SensitiveRule &rule : sensitiveRules()) {
        QRegularExpression &re = *[&]() {
            auto it = compiled.find(rule.pattern);
            if (it == compiled.end()) {
                it = compiled.insert(rule.pattern, QRegularExpression(rule.pattern));
            }
            return &it.value();
        }();
        if (!re.isValid()) {
            continue;
        }
        QRegularExpressionMatchIterator it = re.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            QString value = match.lastCapturedIndex() >= 1 && !match.captured(1).isEmpty()
                                ? match.captured(1)
                                : match.captured(0);
            SensitiveFinding finding;
            finding.id = rule.id;
            finding.sample = redactSecret(value);
            findings.append(finding);
        }
    }

    // Credit cards: 13-19 digits grouped as 4-digit blocks (x space/dash) + Luhn.
    static const QRegularExpression cc(
        QStringLiteral(R"(\b\d{4}(?:[ -]\d{4}){2,4}\b)"));
    QRegularExpressionMatchIterator ccIt = cc.globalMatch(line);
    while (ccIt.hasNext()) {
        const QString raw = ccIt.next().captured(0);
        QString digits;
        for (const QChar &ch : raw) {
            if (ch.isDigit()) {
                digits.append(ch);
            }
        }
        if (luhnValid(digits)) {
            SensitiveFinding finding;
            finding.id = QStringLiteral("credit-card");
            finding.sample = redactSecret(digits);
            findings.append(finding);
        }
    }
    return findings;
}

SensitiveReport scanSensitiveLines(const QStringList &lines) {
    SensitiveReport report;
    report.available = true;
    report.scannedLines = lines.size();
    QHash<QString, int> counts;
    QHash<QString, QString> sample;
    for (const QString &line : lines) {
        for (const SensitiveFinding &finding : scanSensitiveLine(line)) {
            counts[finding.id] += 1;
            ++report.findings;
            if (!sample.contains(finding.id)) {
                sample.insert(finding.id, finding.sample);
            }
        }
    }
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        report.byRule.append(qMakePair(it.key(), it.value()));
        report.samples.append(qMakePair(it.key(), sample.value(it.key())));
    }
    std::sort(report.byRule.begin(), report.byRule.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });
    return report;
}

QString sensitiveSummaryText(const SensitiveReport &report) {
    if (!report.available) {
        return QStringLiteral("sensitive: unavailable");
    }
    return QStringLiteral("%1 finding(s) in %2 lines")
        .arg(report.findings)
        .arg(report.scannedLines);
}

QString sensitiveDetailText(const SensitiveReport &report) {
    if (report.findings == 0) {
        return QStringLiteral("no secrets or PII detected");
    }
    QString text;
    for (const QPair<QString, int> &entry : report.byRule) {
        QString sample;
        for (const QPair<QString, QString> &s : report.samples) {
            if (s.first == entry.first) {
                sample = s.second;
                break;
            }
        }
        text += QStringLiteral("%1  x%2  %3\n")
                    .arg(entry.first.leftJustified(22, ' '))
                    .arg(entry.second)
                    .arg(sample);
    }
    return text.trimmed();
}
