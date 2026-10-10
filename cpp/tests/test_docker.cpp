#include "DockerStatus.h"

#include <cstdio>

namespace {

DockerSnapshot okSnapshot(const QList<DockerContainer> &containers) {
    DockerSnapshot s;
    s.state = DockerState::Ok;
    s.containers = containers;
    s.runningCount = dockerRunningCount(containers);
    s.issueCount = dockerIssueCount(containers);
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

    bool ok = false;
    const QByteArray out = QByteArrayLiteral(
        "id1\tweb\trunning\tUp 2 days (healthy)\n"
        "id2\tdb\trunning\tUp 5 minutes (unhealthy)\n"
        "id3\tjob\trestarting\tRestarting (1) 5 seconds ago\n"
        "id4\told\texited\tExited (0) 2 days ago\n");

    const QList<DockerContainer> containers = parseDockerPs(out, &ok);
    check(ok, "valid tab-separated output parses (ok)");
    check(containers.size() == 4, "parsed four containers");
    check(containers.at(0).id == QStringLiteral("id1"), "parsed id");
    check(containers.at(0).name == QStringLiteral("web"), "parsed name");
    check(containers.at(0).state == QStringLiteral("running"), "parsed state");
    check(containers.at(0).health == QStringLiteral("healthy"),
          "parsed healthy health");
    check(containers.at(1).health == QStringLiteral("unhealthy"),
          "parsed unhealthy health");

    check(dockerRunningCount(containers) == 2,
          "running count excludes restarting/exited");
    check(dockerIssueCount(containers) == 2,
          "issue count = unhealthy + restarting");
    check(!dockerContainerIsIssue(containers.at(0)), "healthy is not an issue");
    check(dockerContainerIsIssue(containers.at(2)), "restarting is an issue");

    bool emptyOk = false;
    check(parseDockerPs(QByteArrayLiteral(""), &emptyOk).isEmpty() && emptyOk,
          "empty output parses to no containers");
    check(parseDockerPs(QByteArrayLiteral("\n\n"), &emptyOk).isEmpty(),
          "blank lines ignored");

    bool malformedOk = true;
    check(parseDockerPs(QByteArrayLiteral("a\tb\tc"), &malformedOk).isEmpty()
              && !malformedOk,
          "malformed (3 fields) -> parse failed");

    check(classifyDockerFailure(
              QByteArrayLiteral("permission denied while trying to connect"))
              == DockerState::PermissionDenied,
          "permission error -> permission denied");
    check(classifyDockerFailure(
              QByteArrayLiteral("Cannot connect to the Docker daemon at "
                                "unix:///var/run/docker.sock."))
              == DockerState::Unreachable,
          "daemon error -> unreachable");
    check(classifyDockerFailure(QByteArrayLiteral("something unexpected went wrong"))
              == DockerState::CommandFailed,
          "other error -> command failed (not unreachable)");

    check(dockerSummaryText(DockerSnapshot{}) == QString(),
          "default (NotInstalled) -> empty/hidden");

    DockerSnapshot unreachable;
    unreachable.state = DockerState::Unreachable;
    check(dockerSummaryText(unreachable)
              == QStringLiteral("docker: daemon unreachable"),
          "unreachable summary");

    DockerSnapshot denied;
    denied.state = DockerState::PermissionDenied;
    check(dockerSummaryText(denied) == QStringLiteral("docker: permission denied"),
          "permission denied summary");

    DockerSnapshot timedOut;
    timedOut.state = DockerState::TimedOut;
    check(dockerSummaryText(timedOut) == QStringLiteral("docker: timed out"),
          "timed out summary");

    DockerSnapshot cmdFailed;
    cmdFailed.state = DockerState::CommandFailed;
    check(dockerSummaryText(cmdFailed)
              == QStringLiteral("docker: command failed"),
          "command failed summary");

    DockerSnapshot parseFailed;
    parseFailed.state = DockerState::ParseFailed;
    check(dockerSummaryText(parseFailed)
              == QStringLiteral("docker: parse failed"),
          "parse failed summary");

    DockerSnapshot oneRunning;
    oneRunning.state = DockerState::Ok;
    oneRunning.runningCount = 1;
    check(dockerSummaryText(oneRunning) == QStringLiteral("docker: 1 running"),
          "ok summary no issues");

    DockerSnapshot oneIssue;
    oneIssue.state = DockerState::Ok;
    oneIssue.runningCount = 2;
    oneIssue.issueCount = 1;
    check(dockerSummaryText(oneIssue)
              == QStringLiteral("docker: 2 running | 1 issue"),
          "ok summary singular issue");

    DockerSnapshot twoIssues;
    twoIssues.state = DockerState::Ok;
    twoIssues.runningCount = 3;
    twoIssues.issueCount = 2;
    check(dockerSummaryText(twoIssues)
              == QStringLiteral("docker: 3 running | 2 issues"),
          "ok summary plural issues");

    DockerSnapshot stale;
    stale.state = DockerState::Stale;
    stale.stale = true;
    stale.runningCount = 1;
    stale.containers = containers;
    check(dockerSummaryText(stale)
              == QStringLiteral("docker: 1 running | stale"),
          "stale summary keeps last counts");

    const auto healthyOnly = okSnapshot(
        parseDockerPs(QByteArrayLiteral("id1\tweb\trunning\tUp (healthy)\n"),
                      &ok));
    check(dockerDetailText(healthyOnly) == QStringLiteral("no issues"),
          "detail text: no issues");

    const auto withIssue = okSnapshot(containers);
    const QString detail = dockerDetailText(withIssue);
    check(detail.contains(QStringLiteral("db")),
          "detail text lists unhealthy container");
    check(detail.contains(QStringLiteral("job")),
          "detail text lists restarting container");
    check(!detail.contains(QStringLiteral("web")),
          "detail text omits healthy container");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
