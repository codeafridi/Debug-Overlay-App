#include "MemoryFormat.h"

#include <QtGlobal>

QString formatMemoryKb(unsigned long long kb) {
    const double mb = static_cast<double>(kb) / 1024.0;

    if (mb >= 1024.0) {
        return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 1);
    }

    return QStringLiteral("%1 MB").arg(qRound(mb));
}
