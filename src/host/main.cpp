// src/host/main.cpp
//
// Margin entry point. main.cpp owns QApplication; HostCore (singleton) owns
// Logger + Settings + SystemTray + the QML dashboard engine, and orchestrates
// bootstrap/shutdown.
//
// Shutdown flows through QCoreApplication::aboutToQuit → HostCore::shutdown()
// for every exit path: tray Quit, --smoke self-quit, OS session logout. The
// dashboard window closes to the tray (not quit), so the app stays resident:
// quitOnLastWindowClosed is disabled here.

#include <QApplication>
#include <QIcon>
#include <QTimer>

#include <cstdio>

#include "core/HostCore.h"

int main(int argc, char* argv[]) {
    // QApplication (not QGuiApplication) is required because SystemTray
    // uses QSystemTrayIcon + QMenu, which live in QtWidgets.
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Margin"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/tray.svg")));
    // Org name intentionally empty: QStandardPaths on Windows concatenates
    // <ORG>/<APP>, so "Margin"/"Margin" double-nests to %APPDATA%\Margin\Margin\.
    // Empty org gives the documented single-nested %APPDATA%\Margin\ layout.
    // macOS .app bundle identifier is set later via macdeployqt -identifier.
    app.setOrganizationName(QString());

    // Tray app: closing the dashboard hides it to the tray rather than quitting
    // (docs/06 §3.2 "任何 → Layer 0"). Quit happens only via the tray Quit item.
    app.setQuitOnLastWindowClosed(false);

    // bootstrap() also loads the QML dashboard (hidden); on failure it has
    // already logged the cause. Tear down explicitly since aboutToQuit does not
    // fire before app.exec(). shutdown() is idempotent.
    if (!Margin::HostCore::instance().bootstrap()) {
        fprintf(stderr, "[FATAL] HostCore::bootstrap failed\n");
        Margin::HostCore::instance().shutdown();
        return 1;
    }

    // aboutToQuit fires once when the event loop exits (any reason). Hook it
    // before app.exec() so normal exit, tray Quit, and --smoke all share the
    // same shutdown path.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        Margin::HostCore::instance().shutdown();
    });

    // CI smoke hook (--smoke → self-quit 500ms post-start). aboutToQuit then
    // drives the full shutdown chain, same as a user clicking Quit.
    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        QTimer::singleShot(500, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
