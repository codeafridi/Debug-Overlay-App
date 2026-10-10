#include "HudWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QtGlobal>

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
        && !qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Debug Overlay"));

    HudWindow hud;
    hud.applyPlaceholderValues(QStringLiteral("example-app"),
                               QStringLiteral("--"),
                               QStringLiteral("--"),
                               QStringLiteral("--"),
                               QStringLiteral("IDLE"));
    hud.show();

    return app.exec();
}
