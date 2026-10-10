#include "KubeStatus.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace {

QString badReason(const KubePod &pod) {
    static const QStringList bad = {
        QStringLiteral("CrashLoopBackOff"),
        QStringLiteral("ImagePullBackOff"),
        QStringLiteral("ErrImagePull"),
        QStringLiteral("CreateContainerConfigError"),
        QStringLiteral("CreateContainerError"),
        QStringLiteral("InvalidImageName"),
        QStringLiteral("RunContainerError"),
        QStringLiteral("OOMKilled"),
        QStringLiteral("Error"),
    };
    return bad.contains(pod.reason) ? pod.reason : QString();
}

} // namespace

QList<KubePod> parseKubectlPods(const QByteArray &json, bool *ok) {
    if (ok != nullptr) {
        *ok = true;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return {};
    }

    const QJsonValue itemsValue = doc.object().value(QStringLiteral("items"));
    if (!itemsValue.isArray()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return {};
    }

    QList<KubePod> pods;
    const QJsonArray items = itemsValue.toArray();
    for (const QJsonValue &itemValue : items) {
        if (!itemValue.isObject()) {
            continue;
        }

        const QJsonObject item = itemValue.toObject();
        const QJsonObject metadata =
            item.value(QStringLiteral("metadata")).toObject();
        const QJsonObject status = item.value(QStringLiteral("status")).toObject();

        KubePod pod;
        pod.ns = metadata.value(QStringLiteral("namespace")).toString();
        pod.name = metadata.value(QStringLiteral("name")).toString();
        pod.phase = status.value(QStringLiteral("phase")).toString();

        const QJsonArray containerStatuses =
            status.value(QStringLiteral("containerStatuses")).toArray();
        for (const QJsonValue &csValue : containerStatuses) {
            if (!csValue.isObject()) {
                continue;
            }
            const QJsonObject cs = csValue.toObject();
            const QJsonObject state = cs.value(QStringLiteral("state")).toObject();

            const int restarts =
                cs.value(QStringLiteral("restartCount")).toInt(0);
            if (restarts > pod.restarts) {
                pod.restarts = restarts;
            }

            const QString waitingReason =
                state.value(QStringLiteral("waiting"))
                    .toObject()
                    .value(QStringLiteral("reason"))
                    .toString();
            const QString terminatedReason =
                state.value(QStringLiteral("terminated"))
                    .toObject()
                    .value(QStringLiteral("reason"))
                    .toString();
            const QString reason = !waitingReason.isEmpty() ? waitingReason
                                                            : terminatedReason;
            if (!reason.isEmpty()) {
                pod.reason = reason;
            }
        }

        pods.append(pod);
    }

    return pods;
}

bool kubePodIsIssue(const KubePod &pod) {
    if (pod.phase == QStringLiteral("Failed")
        || pod.phase == QStringLiteral("Unknown")) {
        return true;
    }
    if (!badReason(pod).isEmpty()) {
        return true;
    }
    return pod.restarts >= 5;
}

int kubeIssueCount(const QList<KubePod> &pods) {
    int issues = 0;
    for (const KubePod &pod : pods) {
        if (kubePodIsIssue(pod)) {
            ++issues;
        }
    }
    return issues;
}

KubeState classifyKubeFailure(const QByteArray &stderr) {
    const QByteArray lower = stderr.toLower();

    if (lower.contains("no configuration")
        || lower.contains("connection refused")
        || lower.contains("was refused")
        || lower.contains("connection to the server")
        || lower.contains("no route to host")
        || lower.contains("unable to connect")
        || lower.contains("couldn't get current server")
        || lower.contains("connection reset")
        || lower.contains("did you specify the right host")) {
        return KubeState::Unreachable;
    }

    return KubeState::CommandFailed;
}

QString kubeSummaryText(const KubeSnapshot &snapshot) {
    switch (snapshot.state) {
    case KubeState::NotConfigured:
        return QString();
    case KubeState::Unreachable:
        return QStringLiteral("k8s: unreachable");
    case KubeState::TimedOut:
        return QStringLiteral("k8s: timed out");
    case KubeState::CommandFailed:
        return QStringLiteral("k8s: command failed");
    case KubeState::ParseFailed:
        return QStringLiteral("k8s: parse failed");
    case KubeState::Stale:
        break;
    case KubeState::Ok:
    default:
        break;
    }

    if (snapshot.issueCount > 0) {
        return QStringLiteral("k8s: %1 %2")
            .arg(snapshot.issueCount)
            .arg(snapshot.issueCount == 1 ? QStringLiteral("issue")
                                          : QStringLiteral("issues"));
    }

    QString text = QStringLiteral("k8s: %1 pods").arg(snapshot.totalPods);
    if (snapshot.state == KubeState::Stale || snapshot.stale) {
        text += QStringLiteral(" | stale");
    }
    return text;
}

QString kubeDetailText(const KubeSnapshot &snapshot) {
    QStringList lines;

    for (const KubePod &pod : snapshot.pods) {
        if (!kubePodIsIssue(pod)) {
            continue;
        }

        QString detail = QStringLiteral("- %1/%2").arg(pod.ns, pod.name);
        if (!pod.phase.isEmpty()) {
            detail += QStringLiteral(" | %1").arg(pod.phase);
        }
        if (!pod.reason.isEmpty()) {
            detail += QStringLiteral(" | %1").arg(pod.reason);
        }
        if (pod.restarts > 0) {
            detail += QStringLiteral(" | r=%1").arg(pod.restarts);
        }
        lines.append(detail);
    }

    if (lines.isEmpty()) {
        return QStringLiteral("no issues");
    }

    return lines.join(QLatin1Char('\n'));
}
