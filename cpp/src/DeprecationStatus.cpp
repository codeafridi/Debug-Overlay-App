#include "DeprecationStatus.h"

#include <algorithm>

const QList<DeprecatedApi> &deprecatedApis() {
    static const QList<DeprecatedApi> apis = [] {
        QList<DeprecatedApi> list;
        auto add = [&list](const char *version, const char *kind,
                           const char *deprecatedIn, const char *removedIn,
                           const char *replacement) {
            DeprecatedApi api;
            api.version = QString::fromLatin1(version);
            api.kind = QString::fromLatin1(kind);
            api.deprecatedIn = QString::fromLatin1(deprecatedIn);
            api.removedIn = QString::fromLatin1(removedIn);
            api.replacement = QString::fromLatin1(replacement);
            list.append(api);
        };
        // Ported from pluto versions.yaml (component: k8s).
        add("extensions/v1beta1", "Deployment", "v1.9.0", "v1.16.0", "apps/v1");
        add("apps/v1beta1", "Deployment", "v1.9.0", "v1.16.0", "apps/v1");
        add("apps/v1beta2", "Deployment", "v1.9.0", "v1.16.0", "apps/v1");
        add("apps/v1beta1", "StatefulSet", "v1.9.0", "v1.16.0", "apps/v1");
        add("apps/v1beta2", "StatefulSet", "v1.9.0", "v1.16.0", "apps/v1");
        add("extensions/v1beta1", "DaemonSet", "v1.9.0", "v1.16.0", "apps/v1");
        add("apps/v1beta2", "DaemonSet", "v1.9.0", "v1.16.0", "apps/v1");
        add("extensions/v1beta1", "ReplicaSet", "", "v1.16.0", "apps/v1");
        add("apps/v1beta1", "ReplicaSet", "", "v1.16.0", "apps/v1");
        add("apps/v1beta2", "ReplicaSet", "", "v1.16.0", "apps/v1");
        add("extensions/v1beta1", "NetworkPolicy", "v1.9.0", "v1.16.0",
            "networking.k8s.io/v1");
        add("extensions/v1beta1", "Ingress", "v1.14.0", "v1.22.0",
            "networking.k8s.io/v1");
        add("networking.k8s.io/v1beta1", "Ingress", "v1.19.0", "v1.22.0",
            "networking.k8s.io/v1");
        add("networking.k8s.io/v1beta1", "IngressClass", "v1.19.0", "v1.22.0",
            "networking.k8s.io/v1");
        add("extensions/v1beta1", "PodSecurityPolicy", "v1.10.0", "v1.16.0",
            "policy/v1beta1");
        add("policy/v1beta1", "PodSecurityPolicy", "v1.21.0", "v1.25.0", "");
        add("policy/v1beta1", "PodDisruptionBudget", "v1.21.0", "v1.25.0",
            "policy/v1");
        add("autoscaling/v2beta1", "HorizontalPodAutoscaler", "v1.22.0",
            "v1.25.0", "autoscaling/v2");
        add("autoscaling/v2beta2", "HorizontalPodAutoscaler", "v1.23.0",
            "v1.26.0", "autoscaling/v2");
        add("batch/v1beta1", "CronJob", "v1.21.0", "v1.25.0", "batch/v1");
        add("node.k8s.io/v1beta1", "RuntimeClass", "v1.22.0", "v1.25.0",
            "node.k8s.io/v1");
        add("scheduling.k8s.io/v1beta1", "PriorityClass", "v1.14.0", "v1.22.0",
            "scheduling.k8s.io/v1");
        add("scheduling.k8s.io/v1alpha1", "PriorityClass", "v1.14.0", "v1.17.0",
            "scheduling.k8s.io/v1");
        add("apiextensions.k8s.io/v1beta1", "CustomResourceDefinition",
            "v1.16.0", "v1.22.0", "apiextensions.k8s.io/v1");
        add("admissionregistration.k8s.io/v1beta1",
            "MutatingWebhookConfiguration", "v1.16.0", "v1.22.0",
            "admissionregistration.k8s.io/v1");
        add("admissionregistration.k8s.io/v1beta1",
            "ValidatingWebhookConfiguration", "v1.16.0", "v1.22.0",
            "admissionregistration.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1beta1", "Role", "v1.17.0", "v1.22.0",
            "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1beta1", "RoleBinding", "v1.17.0",
            "v1.22.0", "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1beta1", "ClusterRole", "v1.17.0",
            "v1.22.0", "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1beta1", "ClusterRoleBinding", "v1.17.0",
            "v1.22.0", "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1alpha1", "Role", "v1.17.0", "v1.22.0",
            "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1alpha1", "RoleBinding", "v1.17.0",
            "v1.22.0", "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1alpha1", "ClusterRole", "v1.17.0",
            "v1.22.0", "rbac.authorization.k8s.io/v1");
        add("rbac.authorization.k8s.io/v1alpha1", "ClusterRoleBinding",
            "v1.17.0", "v1.22.0", "rbac.authorization.k8s.io/v1");
        add("storage.k8s.io/v1beta1", "StorageClass", "v1.6.0", "v1.22.0",
            "storage.k8s.io/v1");
        add("storage.k8s.io/v1beta1", "CSIDriver", "v1.19.0", "v1.22.0",
            "storage.k8s.io/v1");
        add("storage.k8s.io/v1beta1", "CSINode", "v1.17.0", "v1.22.0",
            "storage.k8s.io/v1");
        add("storage.k8s.io/v1beta1", "VolumeAttachment", "v1.13.0", "v1.22.0",
            "storage.k8s.io/v1");
        add("storage.k8s.io/v1beta1", "CSIStorageCapacity", "v1.24.0",
            "v1.27.0", "storage.k8s.io/v1");
        add("apiregistration.k8s.io/v1beta1", "APIService", "v1.10.0",
            "v1.22.0", "apiregistration.k8s.io/v1");
        add("authentication.k8s.io/v1beta1", "TokenReview", "v1.6.0", "v1.22.0",
            "authentication.k8s.io/v1");
        add("certificates.k8s.io/v1beta1", "CertificateSigningRequest",
            "v1.19.0", "v1.22.0", "certificates.k8s.io/v1");
        add("coordination.k8s.io/v1beta1", "Lease", "v1.14.0", "v1.22.0",
            "coordination.k8s.io/v1");
        add("events.k8s.io/v1beta1", "Event", "v1.19.0", "v1.25.0",
            "events.k8s.io/v1");
        add("discovery.k8s.io/v1beta1", "EndpointSlice", "v1.21.0", "v1.25.0",
            "discovery.k8s.io/v1");
        add("audit.k8s.io/v1beta1", "Policy", "v1.21.0", "v1.24.0",
            "audit.k8s.io/v1");
        add("audit.k8s.io/v1beta1", "Event", "v1.21.0", "v1.24.0",
            "audit.k8s.io/v1");
        add("flowcontrol.apiserver.k8s.io/v1beta1", "FlowSchema", "v1.23.0",
            "v1.26.0", "flowcontrol.apiserver.k8s.io/v1beta3");
        add("flowcontrol.apiserver.k8s.io/v1beta2", "FlowSchema", "v1.24.0",
            "v1.29.0", "flowcontrol.apiserver.k8s.io/v1beta3");
        add("flowcontrol.apiserver.k8s.io/v1beta3", "FlowSchema", "v1.24.0",
            "v1.32.0", "flowcontrol.apiserver.k8s.io/v1");
        return list;
    }();
    return apis;
}

QList<DeprecationFinding> checkDeprecations(const QList<ObjectRef> &objects) {
    QList<DeprecationFinding> findings;
    for (const ObjectRef &object : objects) {
        for (const DeprecatedApi &api : deprecatedApis()) {
            if (api.version != object.apiVersion || api.kind != object.kind) {
                continue;
            }
            DeprecationFinding finding;
            finding.apiVersion = object.apiVersion;
            finding.kind = object.kind;
            finding.ns = object.ns;
            finding.name = object.name;
            finding.deprecatedIn = api.deprecatedIn;
            finding.removedIn = api.removedIn;
            finding.replacement = api.replacement;
            finding.removed = !api.removedIn.isEmpty();
            findings.append(finding);
            break;
        }
    }
    std::sort(findings.begin(), findings.end(),
              [](const DeprecationFinding &a, const DeprecationFinding &b) {
                  if (a.removed != b.removed) {
                      return a.removed > b.removed;
                  }
                  return a.kind < b.kind;
              });
    return findings;
}

QString deprecationSummaryText(const QList<DeprecationFinding> &findings) {
    int removed = 0;
    for (const DeprecationFinding &finding : findings) {
        if (finding.removed) {
            ++removed;
        }
    }
    return QStringLiteral("%1 deprecated API object(s)  (%2 removed)")
        .arg(findings.size())
        .arg(removed);
}

QString deprecationDetailText(const QList<DeprecationFinding> &findings) {
    if (findings.isEmpty()) {
        return QStringLiteral("no deprecated/removed Kubernetes APIs in use");
    }
    QString text;
    for (const DeprecationFinding &finding : findings) {
        text += QStringLiteral("%1  %2 %3/%4%5\n")
                    .arg(finding.removed ? QStringLiteral("REMOVED")
                                         : QStringLiteral("DEPRECATED"),
                         finding.apiVersion, finding.kind, finding.name)
                    .arg(finding.replacement.isEmpty()
                             ? QString()
                             : QStringLiteral("  -> %1").arg(finding.replacement));
    }
    return text.trimmed();
}
