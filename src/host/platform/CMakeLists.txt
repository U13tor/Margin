# src/host/platform/CMakeLists.txt
#
# Host-internal platform abstraction. Plugin-facing surface is LockService
# (in margin_plugin_api); concrete impls (PlatformBackendWin / Mac) live
# here and inherit LockService. HostCore owns a unique_ptr<PlatformBackend>
# and passes the raw LockService* down to HostServicesImpl via PluginManager.
#
# Spec: docs/02-source-layout.md §1.1 + docs/08-platform-backends.md §4.

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui)

add_library(margin_platform STATIC
    PlatformBackend.cpp
)

if(WIN32)
    target_sources(margin_platform PRIVATE
        windows/ActiveWindowTrackerWin.cpp
        windows/IdleDetectorWin.cpp
        windows/PlatformBackendWin.cpp
    )
    # M4-C10: DwmSetWindowAttribute for dark title bar.
    target_link_libraries(margin_platform PRIVATE dwmapi)
endif()

target_include_directories(margin_platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
)

target_link_libraries(margin_platform
    PUBLIC  margin_plugin_api   # LockService.h
    PRIVATE Qt6::Core Qt6::Gui  # Qt6::Gui: QWindow in applyDarkTitleBar (M4-C10)
)
