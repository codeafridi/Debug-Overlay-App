#include "NotifierFactory.h"

#include "NullNotifier.h"

#if defined(Q_OS_MACOS)
#include "mac/MacNotifier.h"
#endif

#include <QtGlobal>

std::unique_ptr<Notifier> createNotifier() {
#if defined(Q_OS_LINUX)
    return std::make_unique<DesktopNotifier>();
#elif defined(Q_OS_MACOS)
    return std::make_unique<MacNotifier>();
#else
    return std::make_unique<NullNotifier>();
#endif
}
