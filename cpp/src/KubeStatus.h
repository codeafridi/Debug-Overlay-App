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
    QString lastReason;
    int restarts = 0;
};

struct KubeDeployment {
    QString ns;
    QString name;
    int desired = 0;
    int available = 0;
    int unavailable = 0;
};

struct KubeEvent {
    QString ns;
    QString involvedName;
    QString type;
    QString reason;
    QString message;
    qint64 lastTimestampMs = 0;
    int count = 0;
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

struct KubeResources {
    QList<KubePod> pods;
    QList<KubeDeployment> deployments;
    QList<KubeEvent> events;
};

struct KubeSnapshot {
    KubeState state = KubeState::NotConfigured;
    QList<KubePod> pods;
    QList<KubeDeployment> deployments;
    QList<KubeEvent> events;
    int totalPods = 0;
    int issueCount = 0;
    int eventCount = 0;
    bool stale = false;
    QString errorMessage;
    qint64 sampledAtMs = 0;
};

KubeResources parseKubectlResources(const QByteArray &json, bool *ok);

bool kubePodIsIssue(const KubePod &pod);
bool kubeDeploymentIsIssue(const KubeDeployment &deployment);
bool kubeEventIsWarning(const KubeEvent &event);
int kubeIssueCount(const QList<KubePod> &pods,
                   const QList<KubeDeployment> &deployments);
int kubeWarningCount(const QList<KubeEvent> &events);

KubeState classifyKubeFailure(const QByteArray &stderr);

QString kubeSummaryText(const KubeSnapshot &snapshot);
QString kubeDetailText(const KubeSnapshot &snapshot);
QString kubeEventsText(const KubeSnapshot &snapshot);
QString kubePodsText(const KubeSnapshot &snapshot);
QString kubeDeploymentsText(const KubeSnapshot &snapshot);
