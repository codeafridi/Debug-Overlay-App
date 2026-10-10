#include "DockerStatus.h"

#include <QStringList>

namespace {

QString parseHealth(const QString &status) {
    const QString lower = status.toLower();
    if (lower.contains(QStringLiteral("(unhealthy)"))) {
        return QStringLiteral("unhealthy");
    }
    if (lower.contains(QStringLiteral("health: starting"))
        || lower.contains(QStringLiteral("(starting)"))) {
        return QStringLiteral("starting");
    }
    if (lower.contains(QStringLiteral("(healthy)"))) {
        return QStringLiteral("healthy");
    }
    return QString();
}

QString issueText(int issues) {
    return issues == 1 ? QStringLiteral("issue") : QStringLiteral("issues");
}

} // namespace

QList<DockerContainer> parseDockerPs(const QByteArray &stdout, bool *ok) {
    if (ok != nullptr) {
        *ok = true;
    }

    QList<DockerContainer> containers;
    const QList<QByteArray> lines = stdout.split('\n');

    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QList<QByteArray> fields = line.split('\t');
        if (fields.size() != 4) {
            if (ok != nullptr) {
                *ok = false;
            }
            return {};
        }

        DockerContainer container;
        container.id = QString::fromUtf8(fields.at(0)).trimmed();
        container.name = QString::fromUtf8(fields.at(1)).trimmed();
        container.state = QString::fromUtf8(fields.at(2)).trimmed();
        container.status = QString::fromUtf8(fields.at(3)).trimmed();
        container.health = parseHealth(container.status);
        containers.append(container);
    }

    return containers;
}

bool dockerContainerIsIssue(const DockerContainer &container) {
    if (container.state.compare(QStringLiteral("restarting"),
                                Qt::CaseInsensitive)
        == 0) {
        return true;
    }
    return container.health == QStringLiteral("unhealthy");
}

int dockerRunningCount(const QList<DockerContainer> &containers) {
    int running = 0;
    for (const DockerContainer &container : containers) {
        if (container.state.compare(QStringLiteral("running"),
                                    Qt::CaseInsensitive)
            == 0) {
            ++running;
        }
    }
    return running;
}

int dockerIssueCount(const QList<DockerContainer> &containers) {
    int issues = 0;
    for (const DockerContainer &container : containers) {
        if (dockerContainerIsIssue(container)) {
            ++issues;
        }
    }
    return issues;
}

DockerState classifyDockerFailure(const QByteArray &stderr) {
    const QByteArray lower = stderr.toLower();

    if (lower.contains("permission denied") || lower.contains("access denied")) {
        return DockerState::PermissionDenied;
    }

    if (lower.contains("cannot connect") || lower.contains("failed to connect")
        || lower.contains("is the docker daemon running")
        || lower.contains("no such file or directory")) {
        return DockerState::Unreachable;
    }

    return DockerState::CommandFailed;
}

QString dockerSummaryText(const DockerSnapshot &snapshot) {
    switch (snapshot.state) {
    case DockerState::NotInstalled:
        return QString();
    case DockerState::Starting:
        return QStringLiteral("docker: starting");
    case DockerState::PermissionDenied:
        return QStringLiteral("docker: permission denied");
    case DockerState::Unreachable:
        return QStringLiteral("docker: daemon unreachable");
    case DockerState::TimedOut:
        return QStringLiteral("docker: timed out");
    case DockerState::CommandFailed:
        return QStringLiteral("docker: command failed");
    case DockerState::ParseFailed:
        return QStringLiteral("docker: parse failed");
    case DockerState::Stale:
        break;
    case DockerState::Ok:
    default:
        break;
    }

    const int running = snapshot.runningCount;
    const int issues = snapshot.issueCount;

    QString text = QStringLiteral("docker: %1 running").arg(running);
    if (issues > 0) {
        text += QStringLiteral(" | %1 %2").arg(issues).arg(issueText(issues));
    }
    if (snapshot.state == DockerState::Stale || snapshot.stale) {
        text += QStringLiteral(" | stale");
    }
    return text;
}

QString dockerDetailText(const DockerSnapshot &snapshot) {
    QStringList lines;

    for (const DockerContainer &container : snapshot.containers) {
        if (!dockerContainerIsIssue(container)) {
            continue;
        }

        QString detail = QStringLiteral("- %1").arg(container.name);
        if (!container.state.isEmpty()) {
            detail += QStringLiteral(" | %1").arg(container.state);
        }
        if (!container.status.isEmpty()) {
            detail += QStringLiteral(" | %1").arg(container.status);
        }
        lines.append(detail);
    }

    if (lines.isEmpty()) {
        return QStringLiteral("no issues");
    }

    return lines.join(QLatin1Char('\n'));
}
