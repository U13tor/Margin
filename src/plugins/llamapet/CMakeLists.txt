find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick Network)

add_library(margin_llamapet SHARED
    entry.cpp
    core/EngineConfig.cpp
    core/HalProvider.cpp
    core/NvmlHal.cpp
    core/SmiHal.cpp
    core/LlamaCppIt.cpp
    core/OllamaIt.cpp
    core/OpenAiIt.cpp
    core/EmotionEngine.cpp
    core/Health.cpp
    core/TokenTracker.cpp
    core/TokenStore.cpp
    app/TelemetryService.cpp
    app/ClickThrough.cpp
    app/HotkeyManager.cpp
    app/DockBehavior.cpp
    app/FloatingWindows.cpp
    app/LlamaPetPlugin.cpp
    llamapet.qrc
)

target_link_libraries(margin_llamapet PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
)

margin_plugin_i18n(margin_llamapet llamapet)

target_include_directories(margin_llamapet PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(margin_llamapet PROPERTIES
    OUTPUT_NAME margin-llamapet
    PREFIX ""
)

# Copy dll + manifest next to Margin.exe/plugins/ so PluginManager finds it.
set(_llamapet_plugins_dir "$<TARGET_FILE_DIR:Margin>/plugins")

add_custom_command(TARGET margin_llamapet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_llamapet_plugins_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:margin_llamapet>"
        "${_llamapet_plugins_dir}/$<TARGET_FILE_NAME:margin_llamapet>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/llamapet.manifest.json"
        "${_llamapet_plugins_dir}/margin-llamapet.manifest.json"
)

if(MARGIN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
