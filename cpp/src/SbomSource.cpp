#include "SbomSource.h"

#include <QFile>
#include <QProcess>
#include <QSettings>
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
#if defined(Q_OS_MACOS)
    return resolveMac(paths);
#else
    ensureLoaded();
#endif

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

SbomReport SbomSource::resolveMac(const QStringList &paths) {
    // osquery darwin grounding: apps table reads /Applications bundles'
    // Info.plist keys (CFBundleShortVersionString x6 in strings) and the
    // homebrew_packages table reads Cellar dirs (STR Cellar).
    SbomReport report;
    report.available = true;
    report.libraryCount = paths.size();

    QHash<QPair<QString, QString>, int> index;
    QHash<QString, QString> bundleVersions;
    for (const QString &path : paths) {
        if (path.isEmpty() || !path.startsWith(QLatin1Char('/'))) {
            continue;
        }
        MacPathPackage pkg;
        if (!classifyMacPath(path, pkg)) {
            continue;
        }
        if (pkg.kind == QStringLiteral("bundle")) {
            if (!bundleVersions.contains(pkg.name)) {
                const QString appRoot = path.section(
                    QStringLiteral(".app/"), 0, 0);
                QSettings info(appRoot + QStringLiteral(".app/Contents/Info.plist"),
                               QSettings::NativeFormat);
                bundleVersions.insert(
                    pkg.name,
                    info.value(QStringLiteral("CFBundleShortVersionString"))
                        .toString());
            }
            pkg.version = bundleVersions.value(pkg.name);
        }
        const QPair<QString, QString> key(pkg.name, pkg.version);
        if (!index.contains(key)) {
            SbomEntry entry;
            entry.package = pkg.name;
            entry.version = pkg.version;
            entry.arch = QStringLiteral("universal");
            entry.purl = macPurl(pkg);
            index.insert(key, report.entries.size());
            report.entries.append(entry);
        }
        SbomEntry &entry = report.entries[index.value(key)];
        if (!entry.files.contains(path)) {
            entry.files.append(path);
        }
    }
    report.resolvedCount = report.entries.size();
    return report;
}
