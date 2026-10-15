#pragma once

#include <QList>
#include <QString>

// A Kubernetes object reference (enough to check API deprecation).
struct ObjectRef {
    QString apiVersion;
    QString kind;
    QString ns;
    QString name;
};

struct DeprecatedApi {
    QString version;
    QString kind;
    QString deprecatedIn;
    QString removedIn;
    QString replacement;
};

struct DeprecationFinding {
    QString apiVersion;
    QString kind;
    QString ns;
    QString name;
    QString deprecatedIn;
    QString removedIn;
    QString replacement;
    bool removed = false;
};

// Deprecated/removed Kubernetes API table ported from pluto (Fairwinds),
// as shipped inside the Komodor agent image.
const QList<DeprecatedApi> &deprecatedApis();
QList<DeprecationFinding> checkDeprecations(const QList<ObjectRef> &objects);
QString deprecationSummaryText(const QList<DeprecationFinding> &findings);
QString deprecationDetailText(const QList<DeprecationFinding> &findings);
