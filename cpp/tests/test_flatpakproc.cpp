#include "LinuxProcess.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <unistd.h>

#include <cstdio>

namespace {

QString writeScript(QTemporaryDir &dir, const QString &name, const QString &body) {
    const QString path = dir.path() + QLatin1Char('/') + name;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write("#!/bin/sh\n");
        file.write(body.toUtf8());
        file.close();
    }
    QFile::setPermissions(path,
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                              | QFile::ReadGroup | QFile::ExeGroup
                              | QFile::ReadOther | QFile::ExeOther);
    return path;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    // ---- Host /proc path (simulated Flatpak sandbox) ----
    QTemporaryDir dir;
    check(dir.isValid(), "temp dir created");

    const QString fake =
        QStringLiteral("if [ \"$2\" = \"readlink\" ]; then "
                       "printf '%s\\n' /usr/bin/myapp; exit 0; fi\n"
                       "cat <<'SNAPSHOT'\n"
                       "cpu  100 0 50 1000 0 0 0 0 0 0\n"
                       "P\t1\t0\t10\t5\t3\t2048\tsystemd\n"
                       "P\t42\t1\t100\t20\t99\t4096\tmyapp\n"
                       "SNAPSHOT\n");
    const QString spawn = writeScript(dir, QStringLiteral("fake-spawn"), fake);

    qputenv("FLATPAK_ID", QByteArrayLiteral("io.github.codeafridi.Syspeek"));
    qputenv("SYSPEEK_HOST_SPAWN", spawn.toUtf8());

    check(linuxproc::hostProcEnabled(), "host proc mode enabled in flatpak");

    linuxproc::beginSample();

    unsigned long long total = 0;
    check(linuxproc::totalCpuTicks(total) && total == 1150,
          "total cpu ticks from host dump");

    unsigned long long ticks = 0;
    check(linuxproc::cpuTicks(42, ticks) && ticks == 120,
          "proc cpu ticks from host dump");

    unsigned long long rss = 0;
    check(linuxproc::rssKb(42, rss) && rss == 4096, "rss from host dump");

    check(linuxproc::parentPid(42) == 1, "parent pid from host dump");
    check(linuxproc::startTime(42) == 99, "start time from host dump");
    check(linuxproc::comm(42) == QStringLiteral("myapp"), "comm from host dump");
    check(linuxproc::exePath(42) == QStringLiteral("/usr/bin/myapp"),
          "exe path resolved through the host");

    check(linuxproc::exists(42), "existing pid resolves in host mode");
    check(!linuxproc::exists(999), "missing pid is absent in host mode");

    const QList<int> pids = linuxproc::allPids();
    check(pids.size() == 2 && pids.contains(1) && pids.contains(42),
          "all pids from host dump");

    const ProcessIdentity id = linuxproc::identity(42);
    check(id.isValid() && id.pid == 42 && id.startTime == 99,
          "identity from host dump");

    unsigned long long summed = 0;
    check(linuxproc::sumRssKb({1, 42}, summed) && summed == 6144,
          "summed rss from host dump");

    unsigned long long ignored = 0;
    check(!linuxproc::cpuTicks(999, ignored), "cpu ticks for missing pid fails");
    check(!linuxproc::rssKb(999, ignored), "rss for missing pid fails");

    // A fresh sample must re-run the host command, not reuse stale data.
    linuxproc::beginSample();
    unsigned long long total2 = 0;
    check(linuxproc::totalCpuTicks(total2) && total2 == 1150,
          "second sample refetches the host dump");

    qunsetenv("SYSPEEK_HOST_SPAWN");
    qunsetenv("FLATPAK_ID");

    // ---- Direct /proc path (normal install) ----
    check(!linuxproc::hostProcEnabled(), "host proc mode off outside flatpak");
    check(linuxproc::exists(static_cast<int>(::getpid())),
          "direct /proc read works for our own pid");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
