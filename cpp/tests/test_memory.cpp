#include "LinuxProcess.h"
#include "MemoryFormat.h"

#include <QCoreApplication>

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

    const int self = static_cast<int>(::getpid());
    unsigned long long rss = 0;
    check(linuxproc::rssKb(self, rss), "rssKb(self) read succeeds");
    check(rss > 0, "rssKb(self) is positive");
    check(!linuxproc::rssKb(1 << 30, rss), "rssKb(bogus) fails");

    check(formatMemoryKb(0) == QStringLiteral("0 MB"), "0 KB -> 0 MB");
    check(formatMemoryKb(512ULL * 1024) == QStringLiteral("512 MB"),
          "512 MiB -> 512 MB");
    check(formatMemoryKb(1024ULL * 1024) == QStringLiteral("1.0 GB"),
          "1 GiB -> 1.0 GB");
    check(formatMemoryKb(2ULL * 1024 * 1024 + 512ULL * 1024)
              == QStringLiteral("2.5 GB"),
          "2.5 GiB -> 2.5 GB");

    std::printf("sample rss(self)=%llu kB -> %s\n", rss,
                qUtf8Printable(formatMemoryKb(rss)));

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
