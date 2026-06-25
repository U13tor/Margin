# HostCore orchestrator + SystemTray (QSystemTrayIcon wrapper). main.cpp
# PRIVATE-links margin_core; transitive deps propagate
# (paths/services/plugin_api/Qt6::Core). Qt6::Widgets + Qt6::Qml + Qt6::Quick
# are PRIVATE here: SystemTray.cpp uses QSystemTrayIcon + QMenu, HostCore.cpp
# owns a QQmlApplicationEngine, and AppIconProvider.cpp subclasses
# QQuickImageProvider — none of these types appear in the public headers
# (QQmlApplicationEngine is forward-declared in HostCore.h).
#
# PR3 round-2 #2b: ui/AppIconProvider.{h,cpp} lives in a sibling dir for
# discoverability but compiles into margin_core so HostCore can register
# the provider without a new static lib target.

add_library(margin_core STATIC
    AboutDialog.cpp
    ActivityFeed.cpp
    DashboardTabRegistry.cpp
    HostCore.cpp
    HostGeneralSettings.cpp
    OverlayRegistry.cpp
    PluginManager.cpp
    SettingsRegistry.cpp
    SystemTray.cpp
    ../ui/AppIconProvider.cpp
)

target_link_libraries(margin_core
    PUBLIC margin_services
    PUBLIC margin_paths
    PRIVATE Qt6::Widgets
            Qt6::Qml
            Qt6::Quick
            Qt6::Svg
            margin_security
            margin_platform
)

# Win shell icon extraction (SHGetFileInfoW) lives in shell32; Qt's CMake
# helpers pull it in transitively for QtGui on Windows, but being explicit
# avoids relying on that detail.
if(WIN32)
    target_link_libraries(margin_core PRIVATE shell32)
endif()
