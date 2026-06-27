# src/host/CMakeLists.txt
#
# Margin main executable. main.cpp owns QApplication; HostCore (linked via
# margin_core) owns Logger + Settings + SystemTray. EventBus / PluginManager
# will hang off HostCore in later milestones.

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick Widgets Svg)

add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(security)
add_subdirectory(platform)

set(app_icon_resource "")
set(version_rc "")
if (WIN32)
    set(app_icon_resource "${CMAKE_CURRENT_SOURCE_DIR}/../resources/tray.rc")
    # M6-C1: VERSIONINFO resource (FileVersion / ProductVersion / LegalCopyright
    # shown in Explorer → Properties → Details). configure_file substitutes
    # @PROJECT_VERSION_*@ at configure time; the resulting .rc is compiled
    # alongside tray.rc on the Margin target.
    configure_file(${CMAKE_SOURCE_DIR}/cmake/version.rc.in
                   ${CMAKE_BINARY_DIR}/version.rc @ONLY)
    set(version_rc "${CMAKE_BINARY_DIR}/version.rc")
endif()

qt_add_executable(Margin
    main.cpp
    ../resources/host.qrc
    ../ui/ui.qrc
    ${MARGIN_FONTS_QRC}
    ${MARGIN_I18N_QRC}
    ${app_icon_resource}
    ${version_rc}
)


# WIN32_EXECUTABLE gates the PE subsystem flag (entry point in Margin.exe's
# IMAGE_OPTIONAL_HEADER):
#   Release → WINDOWS subsystem: no console window spawned on double-click
#             (the user-facing experience for distributed binaries).
#   Debug   → CONSOLE subsystem: Logger's stdout mirror stays visible so
#             scripts/build-smoke.sh step 5 can capture the startup line.
# Generator expression keeps the rule in one place — no if/else branching.
set_target_properties(Margin PROPERTIES
    WIN32_EXECUTABLE $<CONFIG:Release>)
# ../resources/host.qrc + ../ui/ui.qrc are processed by AUTORCC
# (CMAKE_AUTORCC ON in top-level CMakeLists). ui.qrc carries the M0-C10a shell
# QML (DashboardWindow/TabBar/StatusBar) at qrc:/ui/.
# ${MARGIN_FONTS_QRC} (M4-C2) is generated at configure time by
# cmake/fetch_fonts.cmake into CMAKE_BINARY_DIR/fonts.qrc — only entries for
# files that actually downloaded successfully are listed, so a network failure
# never breaks the build. HostCore::bootstrap registers whatever faces are
# bundled via QFontDatabase::addApplicationFont.
# Qt6::Widgets: QSystemTrayIcon + QMenu in SystemTray.cpp.
# Qt6::Quick: QtQuick runtime for the QML dashboard (QQmlApplicationEngine + Window).
# Qt6::Svg: QIcon loads tray SVGs via the imageformats svg plugin.
# margin_ui_*plugin: link the STATIC qml module plugins so the Margin.Ui.*
# import URIs resolve for the qrc-loaded shell (docs/06 §6.1).

target_link_libraries(Margin PRIVATE
    margin_core
    margin_ui_primitivesplugin
    margin_ui_compositeplugin
    margin_ui_chartsplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Widgets
    Qt6::Svg
)

# Pull the static qml-module plugin registrations into the executable so the
# linker does not drop their initializers (Margin.Ui.* would otherwise fail to
# import at runtime).
qt_import_qml_plugins(Margin)

# M6-C2: windeployqt is invoked at INSTALL time (see top-level CMakeLists.txt
# install(CODE) block) rather than POST_BUILD. POST_BUILD was tried first but
# broke smoke tests: it deploys platforms/qwindows.dll next to Margin.exe, but
# not platforms/qoffscreen.dll — and tests/smoke/smoke_runner.py runs Margin
# with QT_QPA_PLATFORM=offscreen, so Qt finds only the local platforms/ dir
# (no qoffscreen.dll) and hangs. Keeping the build dir clean lets Qt fall back
# to the Qt install's plugins/ (which has both qwindows.dll and qoffscreen.dll
# via PATH). For CPack, `cmake --install` runs windeployqt against the staging
# prefix where smoke isn't run — no conflict.

# PR1 i18n: guarantee .qm files exist before AUTORCC tries to compile
# ${MARGIN_I18N_QRC}. add_custom_command(OUTPUT) above already makes .qm a
# implicit dependency of the qrc, but this explicit target dep is belt +
# suspenders (docs/15-A6 AUTORCC timing risk).
add_dependencies(Margin ${MARGIN_I18N_QM_TARGET})
