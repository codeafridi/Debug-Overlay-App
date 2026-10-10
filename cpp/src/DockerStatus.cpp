#include "DockerStatus.h"

#include <QList>

DockerCounts countDockerPs(const QByteArray &stdout) {
    DockerCounts counts;

    const QList<QByteArray> lines = stdout.split('\n');
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        ++counts.running;
        if (line.contains("unhealthy")) {
            ++counts.unhealthy;
        }
    }

    return counts;
}

QString formatDockerText(bool installed,
                         bool reachable,
                         const DockerCounts &counts) {
    if (!installed) {
        return QString();
    }

    if (!reachable) {
        return QStringLiteral("docker: unavailable");
    }

    if (counts.unhealthy > 0) {
        return QStringLiteral("docker: %1 running | %2 unhealthy")
            .arg(counts.running)
            .arg(counts.unhealthy);
    }

    return QStringLiteral("docker: %1 running").arg(counts.running);
}
