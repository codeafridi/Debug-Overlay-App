#include "KdeFocusService.h"

#include "KdeFocusState.h"

#include <QDateTime>
#include <QDBusConnection>

KdeFocusService::KdeFocusService(QObject *parent)
    : QObject(parent) {
}

bool KdeFocusService::registerService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }

    if (!bus.registerService(QStringLiteral("org.syspeek.Focus"))) {
        return false;
    }

    return bus.registerObject(QStringLiteral("/Focus"),
                              QStringLiteral("org.syspeek.Focus"),
                              this, QDBusConnection::ExportAllSlots);
}

void KdeFocusService::setFocusedPid(int pid) {
    kdeFocusState().pid.store(pid > 0 ? pid : -1);
    kdeFocusState().stampMs.store(QDateTime::currentMSecsSinceEpoch());
}
