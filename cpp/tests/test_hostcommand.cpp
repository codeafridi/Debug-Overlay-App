#include "HostCommand.h"

#include <QCoreApplication>
#include <QFileInfo>

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

    qunsetenv("FLATPAK_ID");

    check(!hostcmd::inFlatpak(), "host env -> not flatpak");

    const HostCommand plain = hostcmd::wrap(QStringLiteral("swaymsg"),
                                            {QStringLiteral("-t"),
                                             QStringLiteral("get_tree")});
    check(plain.program == QStringLiteral("swaymsg"),
          "non-flatpak program unchanged");
    check(plain.arguments == QStringList({QStringLiteral("-t"),
                                          QStringLiteral("get_tree")}),
          "non-flatpak arguments unchanged");

    const QString resolved = hostcmd::resolveHostProgram(QStringLiteral("sh"));
    check(!resolved.isEmpty() && QFileInfo(resolved).isAbsolute(),
          "non-flatpak resolve returns an absolute path");

    qputenv("FLATPAK_ID", QByteArrayLiteral("io.github.codeafridi.Syspeek"));

    check(hostcmd::inFlatpak(), "FLATPAK_ID -> flatpak");

    const HostCommand wrapped = hostcmd::wrap(QStringLiteral("docker"),
                                              {QStringLiteral("ps"),
                                               QStringLiteral("--all")});
    check(wrapped.program == QStringLiteral("flatpak-spawn"),
          "flatpak program routed through flatpak-spawn");
    check(wrapped.arguments
              == QStringList({QStringLiteral("--host"), QStringLiteral("docker"),
                              QStringLiteral("ps"), QStringLiteral("--all")}),
          "flatpak arguments prefixed with --host and the host program");

    const HostCommand absolute =
        hostcmd::wrap(QStringLiteral("/usr/bin/kubectl"), {QStringLiteral("get")});
    check(absolute.program == QStringLiteral("/usr/bin/kubectl"),
          "absolute program is never wrapped");

    check(hostcmd::resolveHostProgram(QStringLiteral("docker"))
              == QStringLiteral("docker"),
          "flatpak resolve returns the bare name for flatpak-spawn");

    qunsetenv("FLATPAK_ID");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
