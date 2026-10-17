#include "NotifierFactory.h"

#include "NullNotifier.h"

#include <QtGlobal>

std::unique_ptr<Notifier> createNotifier() {
#if defined(Q_OS_LINUX)
    return std::make_unique<DesktopNotifier>();
#else
    return std::make_unique<NullNotifier>();
#endif
}
