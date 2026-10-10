#include "KubeStatus.h"

#include <cstdio>

namespace {

KubeSnapshot snap(const QList<KubePod> &pods,
                  const QList<KubeDeployment> &deployments) {
    KubeSnapshot s;
    s.state = KubeState::Ok;
    s.pods = pods;
    s.deployments = deployments;
    s.totalPods = pods.size();
    s.issueCount = kubeIssueCount(pods, deployments);
    return s;
}

} // namespace

int main() {
    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    const QByteArray json = QByteArrayLiteral(
        "{\"items\":["
        "{\"kind\":\"Pod\",\"metadata\":{\"namespace\":\"default\",\"name\":\"web-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":0,\"state\":{\"running\":{}}}]}},"
        "{\"kind\":\"Pod\",\"metadata\":{\"namespace\":\"default\",\"name\":\"crash-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":7,\"state\":{\"waiting\":{\"reason\":\"CrashLoopBackOff\"}}}]}},"
        "{\"kind\":\"Pod\",\"metadata\":{\"namespace\":\"kube-system\",\"name\":\"failed-1\"},"
        "\"status\":{\"phase\":\"Failed\",\"containerStatuses\":[]}},"
        "{\"kind\":\"Deployment\",\"metadata\":{\"namespace\":\"default\",\"name\":\"nginx\"},"
        "\"spec\":{\"replicas\":3},\"status\":{\"availableReplicas\":1,\"unavailableReplicas\":2}},"
        "{\"kind\":\"Deployment\",\"metadata\":{\"namespace\":\"default\",\"name\":\"ok-dep\"},"
        "\"spec\":{\"replicas\":2},\"status\":{\"availableReplicas\":2}}"
        "]}");

    bool ok = false;
    const KubeResources res = parseKubectlResources(json, &ok);
    check(ok, "valid kubectl json parses");
    check(res.pods.size() == 3, "parsed three pods");
    check(res.deployments.size() == 2, "parsed two deployments");
    check(res.pods.at(1).reason == QStringLiteral("CrashLoopBackOff"),
          "parsed pod waiting reason");
    check(res.pods.at(1).restarts == 7, "parsed pod restart count");

    const KubeDeployment &nginx = res.deployments.at(0);
    check(nginx.name == QStringLiteral("nginx"), "parsed deployment name");
    check(nginx.desired == 3 && nginx.available == 1 && nginx.unavailable == 2,
          "parsed deployment replica counts");
    check(kubeDeploymentIsIssue(nginx), "deployment with unavailable replicas is an issue");
    check(!kubeDeploymentIsIssue(res.deployments.at(1)),
          "deployment fully available is not an issue");

    check(kubeIssueCount(res.pods, res.deployments) == 3,
          "issue count = 2 pods + 1 deployment");

    bool emptyOk = false;
    const KubeResources empty = parseKubectlResources(
        QByteArrayLiteral("{\"items\":[]}"), &emptyOk);
    check(emptyOk && empty.pods.isEmpty() && empty.deployments.isEmpty(),
          "empty items -> nothing");

    bool badOk = true;
    parseKubectlResources(QByteArrayLiteral("not json"), &badOk);
    check(!badOk, "malformed json -> parse failed");

    check(classifyKubeFailure(
              QByteArrayLiteral("The connection to the server was refused"))
              == KubeState::Unreachable,
          "connection refused -> unreachable");
    check(classifyKubeFailure(QByteArrayLiteral("boom"))
              == KubeState::CommandFailed,
          "other error -> command failed");

    const KubeSnapshot summary = snap(res.pods, res.deployments);
    check(kubeSummaryText(summary) == QStringLiteral("k8s: 3 issues"),
          "summary counts pod+deployment issues");
    check(kubeSummaryText(KubeSnapshot{}) == QString(), "NotConfigured hidden");

    KubeSnapshot noIssues;
    noIssues.state = KubeState::Ok;
    noIssues.totalPods = 4;
    check(kubeSummaryText(noIssues) == QStringLiteral("k8s: 4 pods"),
          "summary healthy pod count");

    const QString detail = kubeDetailText(summary);
    check(detail.contains(QStringLiteral("crash-1")), "detail lists crash pod");
    check(detail.contains(QStringLiteral("deploy default/nginx")),
          "detail lists unhealthy deployment");
    check(!detail.contains(QStringLiteral("web-1")), "detail omits healthy pod");
    check(!detail.contains(QStringLiteral("ok-dep")),
          "detail omits healthy deployment");

    const QByteArray oomJson = QByteArrayLiteral(
        "{\"items\":[{\"kind\":\"Pod\",\"metadata\":{\"namespace\":\"d\",\"name\":\"oom-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":0,\"state\":{\"terminated\":{\"reason\":\"OOMKilled\"}}}]}}]}");
    bool oomOk = false;
    const KubeResources oom = parseKubectlResources(oomJson, &oomOk);
    check(oomOk && oom.pods.size() == 1
              && oom.pods.at(0).reason == QStringLiteral("OOMKilled"),
          "parsed terminated reason (OOMKilled)");
    check(kubePodIsIssue(oom.pods.at(0)), "OOMKilled is an issue");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
