#include "HudWindow.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

#include <cstdio>

static DisplaySnapshot makeSnapshot(const char *app,
                                    const char *pid,
                                    const char *cpu,
                                    const char *mem,
                                    const char *status) {
    DisplaySnapshot s;
    s.appName = QString::fromUtf8(app);
    s.pid = QString::fromUtf8(pid);
    s.cpu = QString::fromUtf8(cpu);
    s.memory = QString::fromUtf8(mem);
    s.status = QString::fromUtf8(status);
    return s;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    HudWindow hud;

    int failures = 0;
    auto check = [&failures](bool condition, const char *message) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
        if (!condition) {
            ++failures;
        }
    };

    auto *moreButton = hud.findChild<QPushButton *>(QStringLiteral("moreButton"));
    auto *showHideButton =
        hud.findChild<QPushButton *>(QStringLiteral("showHideButton"));
    auto *freezeButton =
        hud.findChild<QPushButton *>(QStringLiteral("freezeButton"));
    auto *compactLabel = hud.findChild<QLabel *>(QStringLiteral("compact"));
    auto *appLabel = hud.findChild<QLabel *>(QStringLiteral("appValue"));
    auto *pidLabel = hud.findChild<QLabel *>(QStringLiteral("pidValue"));
    auto *cpuLabel = hud.findChild<QLabel *>(QStringLiteral("cpuValue"));
    auto *memLabel = hud.findChild<QLabel *>(QStringLiteral("memoryValue"));
    auto *statusLabel = hud.findChild<QLabel *>(QStringLiteral("status"));

    check(moreButton != nullptr, "MORE/LESS button exists");
    check(showHideButton != nullptr, "HIDE/SHOW button exists");
    check(freezeButton != nullptr, "FRZ/RES button exists");
    check(compactLabel != nullptr, "compact summary label exists");
    check(appLabel != nullptr && pidLabel != nullptr && cpuLabel != nullptr
              && memLabel != nullptr && statusLabel != nullptr,
          "all metric labels exist");
    check(hud.findChild<QLabel *>(QStringLiteral("tick")) == nullptr,
          "temporary tick label removed");

    if (appLabel && pidLabel && cpuLabel && memLabel && statusLabel) {
        hud.setSnapshot(makeSnapshot("alpha", "123", "17%", "8 MB", "WARN"));
        check(appLabel->text() == QStringLiteral("alpha"), "snapshot app rendered");
        check(pidLabel->text() == QStringLiteral("123"), "snapshot pid rendered");
        check(cpuLabel->text() == QStringLiteral("17%"), "snapshot cpu rendered");
        check(memLabel->text() == QStringLiteral("8 MB"), "snapshot mem rendered");
        check(statusLabel->text() == QStringLiteral("WARN"),
              "snapshot status rendered");
        check(compactLabel != nullptr
                  && compactLabel->text().contains(QStringLiteral("alpha")),
              "compact line reflects snapshot");

        QLabel *appPtrBefore = appLabel;
        hud.setSnapshot(makeSnapshot("beta", "999", "3%", "1 MB", "OK"));
        QLabel *appPtrAfter = hud.findChild<QLabel *>(QStringLiteral("appValue"));
        check(appPtrBefore == appPtrAfter,
              "widget not recreated on snapshot update");
        check(appLabel->text() == QStringLiteral("beta"),
              "snapshot update applied");
    }

    if (freezeButton && appLabel) {
        check(freezeButton->text() == QStringLiteral("FRZ"),
              "FRZ/RES starts as FRZ");
        freezeButton->click();
        check(freezeButton->text() == QStringLiteral("RES"),
              "FRZ click -> RES");

        hud.setSnapshot(makeSnapshot("gamma", "7", "5%", "2 MB", "WARN"));
        check(appLabel->text() == QStringLiteral("beta"),
              "frozen: snapshot ignored");

        freezeButton->click();
        check(freezeButton->text() == QStringLiteral("FRZ"),
              "RES click -> FRZ");
        hud.setSnapshot(makeSnapshot("delta", "7", "5%", "2 MB", "WARN"));
        check(appLabel->text() == QStringLiteral("delta"),
              "resume: snapshot applied");
    }

    if (showHideButton && compactLabel) {
        check(showHideButton->text() == QStringLiteral("HIDE"),
              "HIDE/SHOW starts as HIDE");
        showHideButton->click();
        check(showHideButton->text() == QStringLiteral("SHOW"),
              "HIDE click -> SHOW");
        check(compactLabel->isVisibleTo(&hud), "compact summary shown");
        showHideButton->click();
        check(!compactLabel->isVisibleTo(&hud), "compact summary hidden again");
    }

    if (moreButton) {
        check(moreButton->text() == QStringLiteral("MORE"),
              "MORE/LESS starts as MORE");
        moreButton->click();
        check(moreButton->text() == QStringLiteral("LESS"),
              "MORE click -> LESS");
        moreButton->click();
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
