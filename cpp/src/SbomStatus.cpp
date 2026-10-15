#include "SbomStatus.h"

#include <algorithm>

QList<DpkgPackage> parseDpkgStatus(const QByteArray &text) {
    QList<DpkgPackage> packages;
    DpkgPackage current;
    bool has = false;
    auto flush = [&]() {
        if (has && !current.name.isEmpty()) {
            packages.append(current);
        }
        current = DpkgPackage{};
        has = false;
    };

    const QList<QByteArray> lines = text.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromLatin1(raw);
        if (line.trimmed().isEmpty()) {
            flush();
            continue;
        }
        if (line.startsWith(QLatin1Char(' '))) {
            continue; // continuation of a multi-line field
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue;
        }
        const QString key = line.left(colon);
        const QString value = line.mid(colon + 1).trimmed();
        if (key == QStringLiteral("Package")) {
            current.name = value;
            has = true;
        } else if (key == QStringLiteral("Version")) {
            current.version = value;
        } else if (key == QStringLiteral("Architecture")) {
            current.arch = value;
        } else if (key == QStringLiteral("Source")) {
            current.source = value.section(QLatin1Char(' '), 0, 0);
        } else if (key == QStringLiteral("Maintainer")) {
            current.maintainer = value;
        }
    }
    flush();
    return packages;
}

QString makePurl(const DpkgPackage &pkg) {
    QString purl = QStringLiteral("pkg:deb/ubuntu/%1").arg(pkg.name);
    if (!pkg.version.isEmpty()) {
        purl += QStringLiteral("@") + pkg.version;
    }
    if (!pkg.arch.isEmpty()) {
        purl += QStringLiteral("?arch=") + pkg.arch;
    }
    return purl;
}

SbomReport buildSbom(const QStringList &paths,
                     const QHash<QString, DpkgPackage> &packages,
                     const QHash<QString, QString> &fileOwner) {
    SbomReport report;
    report.available = true;
    report.libraryCount = paths.size();

    QHash<QString, SbomEntry> byPackage;
    QStringList order;
    for (const QString &rawPath : paths) {
        QString path = rawPath;
        if (path.endsWith(QStringLiteral(" (deleted)"))) {
            path.chop(10);
        }
        if (path.isEmpty() || !path.startsWith(QLatin1Char('/'))) {
            continue;
        }
        const QString owner = fileOwner.value(path);
        if (owner.isEmpty() || !packages.contains(owner)) {
            continue;
        }
        SbomEntry &entry = byPackage[owner];
        if (entry.package.isEmpty()) {
            const DpkgPackage &pkg = packages.value(owner);
            entry.package = owner;
            entry.version = pkg.version;
            entry.arch = pkg.arch;
            entry.purl = makePurl(pkg);
            order.append(owner);
        }
        if (!entry.files.contains(path)) {
            entry.files.append(path);
        }
    }

    for (const QString &name : order) {
        report.entries.append(byPackage.value(name));
    }
    std::sort(report.entries.begin(), report.entries.end(),
              [](const SbomEntry &a, const SbomEntry &b) {
                  return a.package < b.package;
              });
    report.resolvedCount = report.entries.size();
    return report;
}

QString sbomSummaryText(const SbomReport &report) {
    if (!report.available) {
        return QStringLiteral("sbom: unavailable");
    }
    return QStringLiteral("%1 packages from %2 libraries")
        .arg(report.resolvedCount)
        .arg(report.libraryCount);
}

QString sbomDetailText(const SbomReport &report) {
    if (!report.available) {
        return QStringLiteral("sbom: unavailable");
    }
    if (report.entries.isEmpty()) {
        return QStringLiteral("no package-owned libraries matched");
    }
    QString text;
    for (const SbomEntry &entry : report.entries) {
        text += QStringLiteral("%1@%2  [%3]  %4 lib(s)\n")
                    .arg(entry.package, entry.version, entry.arch)
                    .arg(entry.files.size());
    }
    return text.trimmed();
}
