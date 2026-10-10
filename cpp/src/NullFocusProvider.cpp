#include "NullFocusProvider.h"

QString NullFocusProvider::name() const {
    return QStringLiteral("null");
}

int NullFocusProvider::focusedPid() const {
    return -1;
}
