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

    const QByteArray eventsJson = QByteArrayLiteral(
        "{\"items\":["
        "{\"kind\":\"Event\",\"metadata\":{\"namespace\":\"default\",\"name\":\"web.1\"},"
        "\"involvedObject\":{\"name\":\"web-1\"},\"type\":\"Warning\",\"reason\":\"BackOff\","
        "\"message\":\"Back-off restarting failed container\",\"count\":4,"
        "\"lastTimestamp\":\"2026-10-13T22:00:00Z\"},"
        "{\"kind\":\"Event\",\"metadata\":{\"namespace\":\"default\",\"name\":\"ok.1\"},"
        "\"involvedObject\":{\"name\":\"web-1\"},\"type\":\"Normal\",\"reason\":\"Scheduled\","
        "\"message\":\"Assigned to node\",\"count\":1}]}");
    bool evOk = false;
    const KubeResources ev = parseKubectlResources(eventsJson, &evOk);
    check(evOk && ev.events.size() == 2, "parsed two events");
    check(ev.events.at(0).type == QStringLiteral("Warning")
              && ev.events.at(0).reason == QStringLiteral("BackOff")
              && ev.events.at(0).involvedName == QStringLiteral("web-1")
              && ev.events.at(0).count == 4
              && ev.events.at(0).lastTimestampMs > 0,
          "parsed warning event fields");
    check(kubeWarningCount(ev.events) == 1, "warning event count = 1");

    KubeSnapshot evSnap;
    evSnap.state = KubeState::Ok;
    evSnap.events = ev.events;
    check(kubeEventsText(evSnap).contains(QStringLiteral("Back-off restarting")),
          "events text lists warning");
    check(!kubeEventsText(evSnap).contains(QStringLiteral("Assigned to node")),
          "events text omits normal events");

    const QByteArray lastStateJson = QByteArrayLiteral(
        "{\"items\":[{\"kind\":\"Pod\",\"metadata\":{\"namespace\":\"d\",\"name\":\"restart-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":3,\"state\":{\"running\":{}},"
        "\"lastState\":{\"terminated\":{\"reason\":\"OOMKilled\"}}}]}}]}");
    bool lsOk = false;
    const KubeResources ls = parseKubectlResources(lastStateJson, &lsOk);
    check(lsOk && ls.pods.size() == 1
              && ls.pods.at(0).lastReason == QStringLiteral("OOMKilled"),
          "parsed pod lastState terminated reason");

    // ---- diagnosis ----
    {
        KubePod crash;
        crash.ns = QStringLiteral("default");
        crash.name = QStringLiteral("crash-1");
        crash.phase = QStringLiteral("Running");
        crash.reason = QStringLiteral("CrashLoopBackOff");
        crash.lastReason = QStringLiteral("OOMKilled");
        crash.restarts = 7;
        KubePod pull;
        pull.ns = QStringLiteral("default");
        pull.name = QStringLiteral("pull-1");
        pull.phase = QStringLiteral("Pending");
        pull.reason = QStringLiteral("ImagePullBackOff");
        KubePod healthy;
        healthy.ns = QStringLiteral("default");
        healthy.name = QStringLiteral("ok-1");
        healthy.phase = QStringLiteral("Running");

        KubeEvent ev;
        ev.ns = QStringLiteral("default");
        ev.involvedName = QStringLiteral("pull-1");
        ev.type = QStringLiteral("Warning");
        ev.reason = QStringLiteral("Failed");
        ev.message = QStringLiteral("Failed to pull image: not found");
        ev.count = 3;

        const QList<KubeDiagnosis> diags =
            kubeDiagnose({crash, pull, healthy}, {ev});
        check(diags.size() == 2, "diagnose: two problems");
        check(diags.at(0).title == QStringLiteral("crash loop")
                  && diags.at(0).severity == QStringLiteral("critical")
                  && diags.at(0).detail.contains(QStringLiteral("memory")),
              "diagnose: crash loop -> OOM guidance");
        check(diags.at(1).title == QStringLiteral("image pull failing")
                  && diags.at(1).detail.contains(QStringLiteral("Failed to pull")),
              "diagnose: image pull uses event message");
        check(kubeDiagnosisText(diags).contains(QStringLiteral("crash-1")),
              "diagnosis text lists pod");
        check(kubeDiagnose({healthy}, {}).isEmpty(),
              "diagnose: healthy pod -> none");
    }

    // ---- change timeline ----
    {
        KubeResources prev;
        KubePod pod;
        pod.ns = QStringLiteral("default");
        pod.name = QStringLiteral("web-1");
        pod.phase = QStringLiteral("Running");
        prev.pods = {pod};
        KubeDeployment dep;
        dep.ns = QStringLiteral("default");
        dep.name = QStringLiteral("nginx");
        dep.desired = 3;
        dep.available = 3;
        dep.generation = 1;
        prev.deployments = {dep};

        KubeResources cur = prev;
        cur.pods[0].restarts = 2;
        KubePod created;
        created.ns = QStringLiteral("default");
        created.name = QStringLiteral("new-1");
        created.phase = QStringLiteral("Pending");
        cur.pods.append(created);
        cur.deployments[0].generation = 2;
        cur.deployments[0].available = 1;

        const QList<KubeChange> changes = kubeDiff(prev, cur, 1000);
        bool sawRestart = false;
        bool sawCreated = false;
        bool sawRollout = false;
        bool sawReady = false;
        for (const KubeChange &change : changes) {
            if (change.object == QStringLiteral("web-1")
                && change.summary.contains(QStringLiteral("restarted"))) {
                sawRestart = true;
            }
            if (change.object == QStringLiteral("new-1")
                && change.summary == QStringLiteral("created")) {
                sawCreated = true;
            }
            if (change.object == QStringLiteral("nginx")
                && change.summary.contains(QStringLiteral("rollout"))) {
                sawRollout = true;
            }
            if (change.object == QStringLiteral("nginx")
                && change.summary.contains(QStringLiteral("ready 1/3"))) {
                sawReady = true;
            }
        }
        check(sawRestart && sawCreated && sawRollout && sawReady,
              "diff: restart, create, rollout, readiness captured");
        check(kubeTimelineText(changes).contains(QStringLiteral("web-1")),
              "timeline text lists change");
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
