#pragma once

#include "AlertEngine.h"

#include <QString>

class Notifier {
public:
    virtual ~Notifier() = default;
    virtual QString name() const { return QStringLiteral("notifier"); }
    virtual bool send(const Alert &alert) = 0;
};

// Desktop notification via org.freedesktop.Notifications (the libnotify path).
class DesktopNotifier : public Notifier {
public:
    QString name() const override { return QStringLiteral("desktop"); }
    bool send(const Alert &alert) override;
};
