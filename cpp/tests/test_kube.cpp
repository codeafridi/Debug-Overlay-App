#include "KubeStatus.h"

#include <cstdio>

namespace {

KubeSnapshot snap(KubeState state, const QList<KubePod> &pods) {
    KubeSnapshot s;
    s.state = state;
    s.pods = pods;
    s.totalPods = pods.size();
    s.issueCount = kubeIssueCount(pods);
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
        "{\"metadata\":{\"namespace\":\"default\",\"name\":\"web-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":0,\"state\":{\"running\":{}}}]}},"
        "{\"metadata\":{\"namespace\":\"default\",\"name\":\"crash-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":7,\"state\":{\"waiting\":{\"reason\":\"CrashLoopBackOff\"}}}]}},"
        "{\"metadata\":{\"namespace\":\"kube-system\",\"name\":\"failed-1\"},"
        "\"status\":{\"phase\":\"Failed\",\"containerStatuses\":[]}},"
        "{\"metadata\":{\"namespace\":\"default\",\"name\":\"img-1\"},"
        "\"status\":{\"phase\":\"Pending\",\"containerStatuses\":["
        "{\"restartCount\":0,\"state\":{\"waiting\":{\"reason\":\"ImagePullBackOff\"}}}]}}"
        "]}");

    bool ok = false;
    const QList<KubePod> pods = parseKubectlPods(json, &ok);
    check(ok, "valid kubectl json parses");
    check(pods.size() == 4, "parsed four pods");
    check(pods.at(1).ns == QStringLiteral("default"), "parsed namespace");
    check(pods.at(1).name == QStringLiteral("crash-1"), "parsed name");
    check(pods.at(1).reason == QStringLiteral("CrashLoopBackOff"),
          "parsed waiting reason");
    check(pods.at(1).restarts == 7, "parsed restart count");

    check(kubeIssueCount(pods) == 3, "issue count = crash + failed + imagepull");
    check(!kubePodIsIssue(pods.at(0)), "healthy running pod not an issue");
    check(kubePodIsIssue(pods.at(1)), "crashloopbackoff is an issue");
    check(kubePodIsIssue(pods.at(2)), "failed phase is an issue");
    check(kubePodIsIssue(pods.at(3)), "imagepullbackoff is an issue");

    bool emptyOk = false;
    check(parseKubectlPods(QByteArrayLiteral("{\"items\":[]}"), &emptyOk).isEmpty()
              && emptyOk,
          "empty items -> no pods");

    bool badOk = true;
    check(parseKubectlPods(QByteArrayLiteral("not json"), &badOk).isEmpty()
              && !badOk,
          "malformed json -> parse failed");
    bool badOk2 = true;
    check(parseKubectlPods(QByteArrayLiteral("{\"kind\":\"PodList\"}"), &badOk2)
                  .isEmpty()
              && !badOk2,
          "missing items -> parse failed");

    check(classifyKubeFailure(
              QByteArrayLiteral("The connection to the server 192.168.49.2:8443 "
                                "was refused"))
              == KubeState::Unreachable,
          "connection refused -> unreachable");
    check(classifyKubeFailure(
              QByteArrayLiteral("error: no configuration has been provided"))
              == KubeState::Unreachable,
          "no configuration -> unreachable");
    check(classifyKubeFailure(QByteArrayLiteral("some unexpected failure"))
              == KubeState::CommandFailed,
          "other error -> command failed");

    check(kubeSummaryText(KubeSnapshot{}) == QString(),
          "NotConfigured -> empty/hidden");
    KubeSnapshot unreachable;
    unreachable.state = KubeState::Unreachable;
    check(kubeSummaryText(unreachable) == QStringLiteral("k8s: unreachable"),
          "unreachable summary");
    check(kubeSummaryText(snap(KubeState::Ok, pods))
              == QStringLiteral("k8s: 3 issues"),
          "ok summary issues");
    check(kubeSummaryText(snap(KubeState::Ok, {}))
              == QStringLiteral("k8s: 0 pods"),
          "ok summary no pods");
    KubeSnapshot oneIssue;
    oneIssue.state = KubeState::Ok;
    oneIssue.totalPods = 5;
    oneIssue.issueCount = 1;
    check(kubeSummaryText(oneIssue) == QStringLiteral("k8s: 1 issue"),
          "ok summary singular issue");
    KubeSnapshot stale;
    stale.state = KubeState::Stale;
    stale.stale = true;
    stale.totalPods = 4;
    check(kubeSummaryText(stale).contains(QStringLiteral("stale")),
          "stale summary marks stale");

    const QString detail = kubeDetailText(snap(KubeState::Ok, pods));
    check(detail.contains(QStringLiteral("crash-1")), "detail lists crash pod");
    check(detail.contains(QStringLiteral("img-1")), "detail lists imagepull pod");
    check(!detail.contains(QStringLiteral("web-1")), "detail omits healthy pod");
    check(kubeDetailText(snap(KubeState::Ok, {})) == QStringLiteral("no issues"),
          "detail: no issues");

    const QByteArray oomJson = QByteArrayLiteral(
        "{\"items\":[{\"metadata\":{\"namespace\":\"d\",\"name\":\"oom-1\"},"
        "\"status\":{\"phase\":\"Running\",\"containerStatuses\":["
        "{\"restartCount\":0,\"state\":{\"terminated\":{\"reason\":\"OOMKilled\"}}}]}}]}");
    bool oomOk = false;
    const QList<KubePod> oomPods = parseKubectlPods(oomJson, &oomOk);
    check(oomOk && oomPods.size() == 1
              && oomPods.at(0).reason == QStringLiteral("OOMKilled"),
          "parsed terminated reason (OOMKilled)");
    check(kubePodIsIssue(oomPods.at(0)),
          "OOMKilled is an issue even with 0 restarts");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
