#include "HudWindow.h"
#include "DashboardPanel.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>

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
    auto *compactLabel = hud.findChild<QLabel *>(QStringLiteral("compact"));
    auto *appLabel = hud.findChild<QLabel *>(QStringLiteral("appValue"));
    auto *pidLabel = hud.findChild<QLabel *>(QStringLiteral("pidValue"));
    auto *cpuLabel = hud.findChild<QLabel *>(QStringLiteral("cpuValue"));
    auto *memLabel = hud.findChild<QLabel *>(QStringLiteral("memoryValue"));
    auto *statusLabel = hud.findChild<QLabel *>(QStringLiteral("status"));

    check(moreButton != nullptr, "MORE/LESS button exists");
    check(showHideButton != nullptr, "HIDE/SHOW button exists");
    check(hud.findChild<QPushButton *>(QStringLiteral("freezeButton")) == nullptr,
          "FRZ button removed");
    check(compactLabel != nullptr, "compact summary label exists");
    check(appLabel != nullptr && pidLabel != nullptr && cpuLabel != nullptr
              && memLabel != nullptr && statusLabel != nullptr,
          "all metric labels exist");
    check(hud.findChild<QLabel *>(QStringLiteral("tick")) == nullptr,
          "temporary tick label removed");

    bool hasMemCaption = false;
    for (auto *label : hud.findChildren<QLabel *>()) {
        if (label->text() == QStringLiteral("MEM (RSS)")) {
            hasMemCaption = true;
        }
    }
    check(hasMemCaption, "metrics caption reads MEM (RSS)");
    check(compactLabel != nullptr
              && compactLabel->text().contains(QStringLiteral("MEM(RSS)")),
          "compact line labels memory as MEM(RSS)");

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

    auto *moreButton2 = hud.findChild<QPushButton *>(QStringLiteral("moreButton"));
    auto *stack = hud.findChild<QStackedWidget *>(QStringLiteral("hudStack"));
    auto *panel = hud.findChild<DashboardPanel *>();
    check(panel != nullptr, "dashboard panel exists inside the overlay");
    check(stack != nullptr && stack->currentIndex() == 0,
          "overlay starts on the compact page");
    check(hud.findChild<QLabel *>(QStringLiteral("details")) == nullptr,
          "inline DIAGNOSIS panel removed");

    DisplaySnapshot withProcs = makeSnapshot("alpha", "123", "17%", "8 MB", "OK");
    withProcs.rssKb = 8192;
    withProcs.vmsKb = 9216;
    withProcs.swapKb = 0;
    withProcs.sharedKb = 1024;
    ProcessRow root;
    root.pid = 123;
    root.depth = 0;
    root.name = QStringLiteral("alpha");
    root.memory = QStringLiteral("8 MB");
    ProcessRow child;
    child.pid = 124;
    child.depth = 1;
    child.name = QStringLiteral("alpha-child");
    child.memory = QStringLiteral("4 MB");
    withProcs.processes = {root, child};
    hud.setSnapshot(withProcs);

    auto *dashApp = hud.findChild<QLabel *>(QStringLiteral("ddApp"));
    auto *dashCpu = hud.findChild<QLabel *>(QStringLiteral("ddCpu"));
    auto *dashMem = hud.findChild<QLabel *>(QStringLiteral("ddMem"));
    auto *dashStatus = hud.findChild<QLabel *>(QStringLiteral("ddStatus"));
    auto *dashMemDetail = hud.findChild<QLabel *>(QStringLiteral("ddMemDetail"));
    auto *dashProcs = hud.findChild<QTreeWidget *>(QStringLiteral("ddProcs"));

    check(dashApp != nullptr && dashApp->text() == QStringLiteral("alpha"),
          "panel shows app name");
    check(dashCpu != nullptr && dashCpu->text() == QStringLiteral("17%"),
          "panel shows cpu");
    check(dashMem != nullptr && dashMem->text() == QStringLiteral("8 MB"),
          "panel shows memory");
    check(dashStatus != nullptr && dashStatus->text() == QStringLiteral("OK"),
          "panel shows status");
    check(dashMemDetail != nullptr
              && dashMemDetail->text().contains(QStringLiteral("RSS")),
          "panel shows memory breakdown");
    check(dashProcs != nullptr && dashProcs->topLevelItemCount() == 1,
          "panel process tree has one root");
    check(dashProcs != nullptr && dashProcs->topLevelItem(0) != nullptr
              && dashProcs->topLevelItem(0)->childCount() == 1
              && dashProcs->topLevelItem(0)->child(0)->text(0)
                     == QStringLiteral("alpha-child"),
          "panel process tree nests child");

    DockerSnapshot dockerSnap;
    dockerSnap.state = DockerState::Ok;
    dockerSnap.runningCount = 1;
    dockerSnap.issueCount = 1;
    hud.setDockerSnapshot(dockerSnap);
    KubeSnapshot kubeSnap;
    kubeSnap.state = KubeState::Ok;
    kubeSnap.totalPods = 3;
    kubeSnap.issueCount = 2;
    hud.setKubeSnapshot(kubeSnap);
    auto *dashDocker = hud.findChild<QLabel *>(QStringLiteral("ddDocker"));
    auto *dashKube = hud.findChild<QLabel *>(QStringLiteral("ddKube"));
    check(dashDocker != nullptr
              && dashDocker->text().contains(QStringLiteral("docker:")),
          "panel shows docker");
    check(dashKube != nullptr && dashKube->text().contains(QStringLiteral("k8s:")),
          "panel shows kubernetes");

    if (moreButton2 && stack) {
        moreButton2->click();
        check(stack->currentIndex() == 1, "MORE expands the overlay in place");
        check(moreButton2->text() == QStringLiteral("LESS"), "MORE -> LESS");
        moreButton2->click();
        check(stack->currentIndex() == 0, "LESS collapses the overlay in place");
        check(moreButton2->text() == QStringLiteral("MORE"), "LESS -> MORE");
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
