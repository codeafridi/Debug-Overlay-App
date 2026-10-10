#include "DockerStatus.h"

#include <cstdio>

int main() {
    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    {
        const DockerCounts counts = countDockerPs(
            QByteArrayLiteral("Up 2 days (healthy)\n"));
        check(counts.running == 1, "one healthy container counted");
        check(counts.unhealthy == 0, "no unhealthy counted");
    }

    {
        const DockerCounts counts = countDockerPs(QByteArrayLiteral(
            "Up 2 days (healthy)\n"
            "Up 5 minutes (unhealthy)\n"
            "Restarting (1) 5 seconds ago\n"));
        check(counts.running == 3, "three running lines counted");
        check(counts.unhealthy == 1, "one unhealthy line counted");
    }

    {
        const DockerCounts counts = countDockerPs(QByteArrayLiteral("\n\n"));
        check(counts.running == 0, "blank lines ignored");
    }

    check(formatDockerText(false, false, {}) == QString(),
          "not installed -> empty (hidden)");
    check(formatDockerText(true, false, {}) == QStringLiteral("docker: unavailable"),
          "installed but unreachable -> unavailable");

    DockerCounts none;
    check(formatDockerText(true, true, none) == QStringLiteral("docker: 0 running"),
          "reachable, none running");

    DockerCounts one;
    one.running = 1;
    check(formatDockerText(true, true, one) == QStringLiteral("docker: 1 running"),
          "reachable, one running");

    DockerCounts bad;
    bad.running = 2;
    bad.unhealthy = 1;
    check(formatDockerText(true, true, bad)
              == QStringLiteral("docker: 2 running | 1 unhealthy"),
          "reachable, one unhealthy");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
