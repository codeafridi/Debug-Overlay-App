#include "FocusSampler.h"

#include "FocusedWindowProvider.h"
#include "LinuxProcess.h"

#include <unistd.h>

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
    const int self = static_cast<int>(::getpid());

    // Never report Syspeek's own overlay as the focused app. Clicking the
    // overlay focuses it on some compositors; fall back to the last real app.
    if (pid == self) {
        if (m_lastIdentity.isValid()) {
            result.available = true;
            result.identity = m_lastIdentity;
            result.appName = m_lastName;
        }
        return result;
    }

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

    m_lastIdentity = identity;
    m_lastName = result.appName;

    return result;
}
