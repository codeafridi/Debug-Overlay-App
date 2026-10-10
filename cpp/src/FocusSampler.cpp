#include "FocusSampler.h"

#include "FocusedWindowProvider.h"
#include "LinuxProcess.h"

FocusSampler::FocusSampler(const FocusedWindowProvider *provider)
    : m_provider(provider) {
}

FocusSample FocusSampler::sample() const {
    FocusSample result;

    if (m_provider == nullptr) {
        return result;
    }

    result.providerName = m_provider->name();

    const int pid = m_provider->focusedPid();
    if (pid <= 0) {
        return result;
    }

    const ProcessIdentity identity = linuxproc::identity(pid);
    if (!identity.isValid()) {
        return result;
    }

    result.available = true;
    result.identity = identity;
    result.appName = linuxproc::comm(pid);
    if (result.appName.isEmpty()) {
        result.appName = QStringLiteral("pid %1").arg(pid);
    }

    return result;
}
