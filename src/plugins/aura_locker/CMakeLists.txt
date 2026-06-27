# Aura Locker plugin — Bluetooth proximity auto-lock.
# Spec: docs/04-plugin-spec.md §7 (Contributors) + docs/11-roadmap.md M1.
#
# Output: margin-aura-locker.dll sitting next to Margin.exe under
# <build>/src/host/plugins/ so PluginManager::loadAll picks it up.

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Qml)

add_library(margin_aura_locker SHARED
    entry.cpp
    AuraLockerPlugin.cpp
    aura_locker.qrc
    BluetoothProximityTracker.cpp
    ProximityDetector.cpp
    RssiSmoother.cpp
)

target_link_libraries(margin_aura_locker PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Qml
)

if(WIN32)
    # C++/WinRT Windows impl. cppwinrt headers + windowsapp runtime stubs
    # are provided by the margin_cppwinrt_iface INTERFACE library defined
    # at the top-level CMakeLists.txt (shared with the unit test).
    target_sources(margin_aura_locker PRIVATE win/BluetoothProximityTrackerWin.cpp)
    target_link_libraries(margin_aura_locker PRIVATE margin_cppwinrt_iface)
endif()

# PR2: per-plugin i18n catalog. Plugin runtime id is "aura" (kId in
# AuraLockerPlugin.cpp); folder/target is named "aura_locker" to match
# the binary margin-aura-locker.dll. margin_plugin_i18n takes the
# RUNTIME plugin_id so the catalog lands at :/aura/i18n/aura_<lang>.qm
# (matching what PluginManager::loadPluginTranslator looks up).
margin_plugin_i18n(margin_aura_locker aura)

# Output name 'margin-aura-locker' (CLAUDE.md §4 plugin binary naming).
set_target_properties(margin_aura_locker PROPERTIES
    OUTPUT_NAME margin-aura-locker
    PREFIX ""
)

# Copy dll + manifest next to Margin.exe/plugins/ so PluginManager finds it.
set(_aura_plugins_dir "$<TARGET_FILE_DIR:Margin>/plugins")

add_custom_command(TARGET margin_aura_locker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_aura_plugins_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:margin_aura_locker>"
        "${_aura_plugins_dir}/$<TARGET_FILE_NAME:margin_aura_locker>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
        "${_aura_plugins_dir}/margin-aura-locker.manifest.json"
)
