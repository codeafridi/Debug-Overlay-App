#include "HudWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
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
