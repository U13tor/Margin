# Rhythm & Health plugin — Pomodoro timer + guided stretch breaks.
# Spec: docs/04-plugin-spec.md §8.4 + docs/11-roadmap.md M3.
#
# Output: margin-rhythm.dll sitting next to Margin.exe under
# <build>/src/host/plugins/ so PluginManager::loadAll picks it up.
#
# Lottie (Qt6LabsLottie) for the M3-C4 break animation is deferred — see
# docs/12-deferred-items.md §A22 (build host lacks the module). BreakOverlay.qml
# uses an SVG + QML NumberAnimation fallback, so no extra Qt component is linked.

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick)

add_library(margin_rhythm SHARED
    entry.cpp
    RhythmPlugin.cpp
    PomodoroTimer.cpp
    rhythm.qrc
)

target_link_libraries(margin_rhythm PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

# PR2: per-plugin i18n catalog. Ships i18n/rhythm_{en,zh_CN}.ts at
# :/rhythm/i18n/rhythm_<lang>.qm inside the DLL. HostCore's
# PluginManager::setLanguage loads these on plugin load + language change.
margin_plugin_i18n(margin_rhythm rhythm)

# Output name 'margin-rhythm' (CLAUDE.md §4 plugin binary naming).
set_target_properties(margin_rhythm PROPERTIES
    OUTPUT_NAME margin-rhythm
    PREFIX ""
)

# Copy dll + manifest next to Margin.exe/plugins/ so PluginManager finds it.
set(_rhythm_plugins_dir "$<TARGET_FILE_DIR:Margin>/plugins")

add_custom_command(TARGET margin_rhythm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_rhythm_plugins_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:margin_rhythm>"
        "${_rhythm_plugins_dir}/$<TARGET_FILE_NAME:margin_rhythm>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
        "${_rhythm_plugins_dir}/margin-rhythm.manifest.json"
)
