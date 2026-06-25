# Screen Time plugin — passive foreground-app duration tracker.
# Spec: docs/04-plugin-spec.md §8.3 + docs/11-roadmap.md M2.
#
# Output: margin-screen-time.dll sitting next to Margin.exe under
# <build>/src/host/plugins/ so PluginManager::loadAll picks it up.
#
# M2-C8 ships only the skeleton: manifest + multi-inherit shell + a
# stub QML tab. M2-C3 wires DB; M2-C5 adds CategoryMatcher;
# M2-C6/C7 fill out the UI surface.

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Qml Sql)

add_library(margin_screen_time SHARED
    entry.cpp
    ScreenTimePlugin.cpp
    SessionStore.cpp
    CategoryMatcher.cpp
    screen_time.qrc
)

target_link_libraries(margin_screen_time PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Qml
    Qt6::Sql
)

# PR2: per-plugin i18n catalog. Ships i18n/screen_time_{en,zh_CN}.ts at
# :/screen_time/i18n/screen_time_<lang>.qm inside the DLL. HostCore's
# PluginManager::setLanguage loads these on plugin load + language change.
margin_plugin_i18n(margin_screen_time screen_time)

# Output name 'margin-screen-time' (CLAUDE.md §4 plugin binary naming).
set_target_properties(margin_screen_time PROPERTIES
    OUTPUT_NAME margin-screen-time
    PREFIX ""
)

# Copy dll + manifest next to Margin.exe/plugins/ so PluginManager finds it.
set(_screen_time_plugins_dir "$<TARGET_FILE_DIR:Margin>/plugins")

add_custom_command(TARGET margin_screen_time POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_screen_time_plugins_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:margin_screen_time>"
        "${_screen_time_plugins_dir}/$<TARGET_FILE_NAME:margin_screen_time>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
        "${_screen_time_plugins_dir}/margin-screen-time.manifest.json"
)
