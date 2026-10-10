#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

struct KubePod {
    QString ns;
    QString name;
    QString phase;
    QString reason;
    int restarts = 0;
};

enum class KubeState {
    NotConfigured,
    Unreachable,
    Ok,
    TimedOut,
    CommandFailed,
    ParseFailed,
    Stale,
};

struct KubeSnapshot {
    KubeState state = KubeState::NotConfigured;
    QList<KubePod> pods;
    int totalPods = 0;
    int issueCount = 0;
    bool stale = false;
    QString errorMessage;
    qint64 sampledAtMs = 0;
};

QList<KubePod> parseKubectlPods(const QByteArray &json, bool *ok);

bool kubePodIsIssue(const KubePod &pod);
int kubeIssueCount(const QList<KubePod> &pods);

KubeState classifyKubeFailure(const QByteArray &stderr);

QString kubeSummaryText(const KubeSnapshot &snapshot);
QString kubeDetailText(const KubeSnapshot &snapshot);
