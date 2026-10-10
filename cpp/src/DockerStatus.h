#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct DockerContainer {
    QString id;
    QString name;
    QString state;
    QString status;
    QString health;
};

enum class DockerState {
    NotInstalled,
    Starting,
    Ok,
    PermissionDenied,
    Unreachable,
    TimedOut,
    CommandFailed,
    ParseFailed,
    Stale,
};

struct DockerSnapshot {
    DockerState state = DockerState::NotInstalled;
    QList<DockerContainer> containers;
    int runningCount = 0;
    int issueCount = 0;
    bool stale = false;
    QString errorMessage;
    qint64 sampledAtMs = 0;
};

QList<DockerContainer> parseDockerPs(const QByteArray &stdout, bool *ok);

int dockerRunningCount(const QList<DockerContainer> &containers);
int dockerIssueCount(const QList<DockerContainer> &containers);
bool dockerContainerIsIssue(const DockerContainer &container);

DockerState classifyDockerFailure(const QByteArray &stderr);

QString dockerSummaryText(const DockerSnapshot &snapshot);
QString dockerDetailText(const DockerSnapshot &snapshot);
