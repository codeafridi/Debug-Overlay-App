#pragma once

#include <QByteArray>
#include <QString>

struct DockerCounts {
    int running = 0;
    int unhealthy = 0;
};

DockerCounts countDockerPs(const QByteArray &stdout);

QString formatDockerText(bool installed, bool reachable, const DockerCounts &counts);
