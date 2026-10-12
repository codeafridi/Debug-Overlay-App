#include "LinuxProcess.h"
#include "MemoryFormat.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QHash>
#include <QList>

#include <unistd.h>

#include <cstdio>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    // ---- format ----
    check(formatMemoryKb(0) == QStringLiteral("0 MB"), "0 KB -> 0 MB");
    check(formatMemoryKb(512ULL * 1024) == QStringLiteral("512 MB"),
          "512 MiB -> 512 MB");
    check(formatMemoryKb(1024ULL * 1024) == QStringLiteral("1.0 GB"),
          "1 GiB -> 1.0 GB");
    check(formatMemoryKb(2ULL * 1024 * 1024 + 512ULL * 1024)
              == QStringLiteral("2.5 GB"),
          "2.5 GiB -> 2.5 GB");

    // ---- VmRSS parser edge cases ----
    {
        unsigned long long kb = 0;
        check(linuxproc::parseVmRssKb("Name:\tcat\nVmRSS:\t   2576 kB\n", kb)
                  && kb == 2576,
              "parser: valid VmRSS line");
        check(!linuxproc::parseVmRssKb("Name:\tcat\nVmSize:\t100 kB\n", kb),
              "parser: missing VmRSS -> false");
        check(!linuxproc::parseVmRssKb("VmRSS:\tabc kB\n", kb),
              "parser: non-numeric VmRSS -> false");
        check(!linuxproc::parseVmRssKb("", kb), "parser: empty content -> false");
        check(!linuxproc::parseVmRssKb("VmRSS:\n", kb),
              "parser: VmRSS without value -> false");
        check(linuxproc::parseVmRssKb("VmRSS:\t0 kB\n", kb) && kb == 0,
              "parser: VmRSS 0 is valid (not absence)");
        check(linuxproc::parseVmRssKb("VmRSS:  12345   kB\n", kb) && kb == 12345,
              "parser: extra whitespace tolerated");
        check(linuxproc::parseVmRssKb("VmRSS:\t100 kB\r\n", kb) && kb == 100,
              "parser: CRLF line ending tolerated");
        check(linuxproc::parseVmRssKb("VmRSS:\t4096 kB\n", kb) && kb == 4096,
              "parser: tab separator tolerated");
        check(linuxproc::parseVmRssKb("VmRSS:\t18446744073709551615 kB\n", kb)
                  && kb == 18446744073709551615ULL,
              "parser: 64-bit max value tolerated");
    }

    // ---- real /proc ----
    const int self = static_cast<int>(::getpid());
    unsigned long long rss = 0;
    check(linuxproc::rssKb(self, rss), "rssKb(self) read succeeds");
    check(rss > 0, "rssKb(self) is positive");
    check(!linuxproc::rssKb(1 << 30, rss), "rssKb(bogus) fails");

    // ---- group sum (deterministic seam) ----
    {
        const QHash<int, unsigned long long> table{{10, 100}, {20, 250}};
        linuxproc::RssReader reader = [&table](int pid, unsigned long long &out) {
            const auto it = table.constFind(pid);
            if (it == table.constEnd()) {
                return false;
            }
            out = it.value();
            return true;
        };

        unsigned long long total = 0;
        check(linuxproc::sumRssKbWith(QList<int>{10}, reader, total) && total == 100,
              "sumRssKbWith single pid");
        check(linuxproc::sumRssKbWith(QList<int>{10, 20}, reader, total)
                  && total == 350,
              "sumRssKbWith two pids summed");
        check(linuxproc::sumRssKbWith(QList<int>{10, 999}, reader, total)
                  && total == 100,
              "sumRssKbWith skips unreadable pid, keeps valid");
        check(!linuxproc::sumRssKbWith(QList<int>{999}, reader, total),
              "sumRssKbWith all unreadable -> false");
        check(!linuxproc::sumRssKbWith(QList<int>{}, reader, total),
              "sumRssKbWith empty -> false");
        check(linuxproc::sumRssKbWith(QList<int>{10, 10}, reader, total)
                  && total == 200,
              "sumRssKbWith double-counts shared pages (caveat documented)");
    }

    // ---- group sum (real /proc) ----
    {
        unsigned long long total = 0;
        check(linuxproc::sumRssKb(QList<int>{self}, total) && total > 0,
              "sumRssKb(self) reads a positive group total");
        check(!linuxproc::sumRssKb(QList<int>{1 << 30}, total),
              "sumRssKb(all unreadable) -> false");
    }

    std::printf("sample rss(self)=%llu kB -> %s\n", rss,
                qUtf8Printable(formatMemoryKb(rss)));

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
