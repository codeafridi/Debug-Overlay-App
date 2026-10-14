#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QtGlobal>

struct KubePod {
    QString ns;
    QString name;
    QString phase;
    QString reason;
    QString lastReason;
    int restarts = 0;
    double cpuRequestCores = 0.0;
    double memRequestBytes = 0.0;
    double cpuLimitCores = 0.0;
    double memLimitBytes = 0.0;
    int containersWithoutCpuRequest = 0;
    int containersWithoutMemRequest = 0;
};

struct KubeDeployment {
    QString ns;
    QString name;
    int desired = 0;
    int available = 0;
    int unavailable = 0;
    int generation = 0;
};

struct KubeDiagnosis {
    QString ns;
    QString pod;
    QString severity;
    QString title;
    QString detail;
};

struct KubeChange {
    qint64 atMs = 0;
    QString ns;
    QString object;
    QString kind;
    QString summary;
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
    QList<KubeDiagnosis> diagnoses;
    QList<KubeChange> timeline;
    int totalPods = 0;
    int issueCount = 0;
    int eventCount = 0;
    double costMonthly = 0.0;
    int costMissingRequests = 0;
    int costPods = 0;
    QList<QPair<QString, double>> costByNamespace;
    QList<QPair<QString, double>> costTopWorkloads;
    bool stale = false;
    QString errorMessage;
    qint64 sampledAtMs = 0;
};

KubeResources parseKubectlResources(const QByteArray &json, bool *ok);

double kubeParseCpuCores(const QString &quantity);
double kubeParseMemoryBytes(const QString &quantity);

QList<KubeDiagnosis> kubeDiagnose(const QList<KubePod> &pods,
                                  const QList<KubeEvent> &events);
QList<KubeChange> kubeDiff(const KubeResources &prev, const KubeResources &cur,
                           qint64 nowMs);

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
QString kubeDiagnosisText(const QList<KubeDiagnosis> &diagnoses);
QString kubeTimelineText(const QList<KubeChange> &changes);
