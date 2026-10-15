#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct DpkgPackage {
    QString name;
    QString version;
    QString arch;
    QString source;
    QString maintainer;
};

struct SbomEntry {
    QString package;
    QString version;
    QString arch;
    QString purl;
    QStringList files;
};

struct SbomReport {
    bool available = false;
    int libraryCount = 0;
    int resolvedCount = 0;
    QList<SbomEntry> entries;
};

// Parse /var/lib/dpkg/status into packages.
QList<DpkgPackage> parseDpkgStatus(const QByteArray &text);

// purl for a Debian package: pkg:deb/ubuntu/<name>@<version>?arch=<arch>
QString makePurl(const DpkgPackage &pkg);

// Resolve filesystem paths to owning packages and build the SBOM.
SbomReport buildSbom(const QStringList &paths,
                     const QHash<QString, DpkgPackage> &packages,
                     const QHash<QString, QString> &fileOwner);

QString sbomSummaryText(const SbomReport &report);
QString sbomDetailText(const SbomReport &report);
