#include "HudWindow.h"

#include <QApplication>
#include <QPixmap>

#include <cstdio>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    HudWindow hud;

    DisplaySnapshot display;
    display.appName = QStringLiteral("cursor");
    display.pid = QStringLiteral("2092707");
    display.cpu = QStringLiteral("12%");
    display.memory = QStringLiteral("212 MB");
    display.status = QStringLiteral("OK");
    hud.setSnapshot(display);

    DockerSnapshot docker;
    docker.state = DockerState::Ok;
    docker.runningCount = 2;
    docker.issueCount = 1;
    DockerContainer db;
    db.id = QStringLiteral("c1");
    db.name = QStringLiteral("postgres");
    db.state = QStringLiteral("running");
    db.status = QStringLiteral("Up 5 minutes (unhealthy)");
    db.health = QStringLiteral("unhealthy");
    DockerContainer web;
    web.id = QStringLiteral("c2");
    web.name = QStringLiteral("web");
    web.state = QStringLiteral("running");
    web.status = QStringLiteral("Up 2 days (healthy)");
    web.health = QStringLiteral("healthy");
    docker.containers = {db, web};
    hud.setDockerSnapshot(docker);

    KubeSnapshot kube;
    kube.state = KubeState::Ok;
    kube.totalPods = 12;
    kube.issueCount = 2;
    KubePod pod;
    pod.ns = QStringLiteral("default");
    pod.name = QStringLiteral("api-7f9c");
    pod.phase = QStringLiteral("Running");
    pod.reason = QStringLiteral("CrashLoopBackOff");
    pod.restarts = 7;
    kube.pods = {pod};
    hud.setKubeSnapshot(kube);

    hud.show();
    app.processEvents();

    const QString out = argc > 1 ? QString::fromUtf8(argv[1])
                                 : QStringLiteral("hud.png");
    const bool ok = hud.grab().save(out);
    std::printf("%s: %s\n", ok ? "saved" : "failed",
                out.toUtf8().constData());
    return ok ? 0 : 1;
}
