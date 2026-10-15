#include "SbomSource.h"

#include <QFile>
#include <QProcess>
#include <QStringList>

void SbomSource::ensureLoaded() {
    if (m_loaded) {
        return;
    }
    m_loaded = true;
    QFile status(QStringLiteral("/var/lib/dpkg/status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return;
    }
    const QList<DpkgPackage> packages = parseDpkgStatus(status.readAll());
    for (const DpkgPackage &pkg : packages) {
        m_packages.insert(pkg.name, pkg);
    }
}

SbomReport SbomSource::resolve(const QStringList &paths) {
    ensureLoaded();

    QStringList unknown;
    for (const QString &path : paths) {
        if (!path.startsWith(QLatin1Char('/'))) {
            continue;
        }
        if (!m_fileOwner.contains(path)) {
            unknown.append(path);
        }
    }

    // Batch-resolve owners for unseen paths via dpkg-query (cached after).
    for (int i = 0; i < unknown.size(); i += 100) {
        const QStringList chunk = unknown.mid(i, 100);
        QProcess process;
        QStringList args{QStringLiteral("-S")};
        args.append(chunk);
        process.start(QStringLiteral("dpkg-query"), args);
        if (!process.waitForStarted(1000)
            || !process.waitForFinished(5000)) {
            break;
        }
        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        for (const QString &line : output.split(QLatin1Char('\n'))) {
            const int sep = line.indexOf(QStringLiteral(": "));
            if (sep < 0) {
                continue;
            }
            QString owner = line.left(sep);
            const QString file = line.mid(sep + 2).trimmed();
            const int archSep = owner.indexOf(QLatin1Char(':'));
            if (archSep >= 0) {
                owner = owner.left(archSep);
            }
            if (file.startsWith(QLatin1Char('/'))) {
                m_fileOwner.insert(file, owner);
            }
        }
        // Mark unresolved paths so we do not re-query them.
        for (const QString &path : chunk) {
            if (!m_fileOwner.contains(path)) {
                m_fileOwner.insert(path, QString());
            }
        }
    }

    return buildSbom(paths, m_packages, m_fileOwner);
}
