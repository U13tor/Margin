find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Quick Network Test)

set(_fixtures_dir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# 1. tst_smi_parse
add_executable(tst_smi_parse
    unit/tst_smi_parse.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/SmiHal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/NvmlHal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/HalProvider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EngineConfig.cpp"
)
target_link_libraries(tst_smi_parse PRIVATE margin_plugin_api Qt6::Core Qt6::Test)
target_include_directories(tst_smi_parse PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_smi_parse PROPERTIES AUTOMOC ON)
add_test(NAME tst_smi_parse COMMAND tst_smi_parse)
set_tests_properties(tst_smi_parse PROPERTIES LABELS "unit")

# 2. tst_slots_parse
add_executable(tst_slots_parse
    unit/tst_slots_parse.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/LlamaCppIt.cpp"
)
target_link_libraries(tst_slots_parse PRIVATE margin_plugin_api Qt6::Core Qt6::Network Qt6::Test)
target_include_directories(tst_slots_parse PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(tst_slots_parse PRIVATE FIXTURES_DIR="${_fixtures_dir}")
set_target_properties(tst_slots_parse PROPERTIES AUTOMOC ON)
add_test(NAME tst_slots_parse COMMAND tst_slots_parse)
set_tests_properties(tst_slots_parse PROPERTIES LABELS "unit")

# 3. tst_ollama_parse
add_executable(tst_ollama_parse
    unit/tst_ollama_parse.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/OllamaIt.cpp"
)
target_link_libraries(tst_ollama_parse PRIVATE margin_plugin_api Qt6::Core Qt6::Test)
target_include_directories(tst_ollama_parse PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(tst_ollama_parse PRIVATE FIXTURES_DIR="${_fixtures_dir}")
set_target_properties(tst_ollama_parse PROPERTIES AUTOMOC ON)
add_test(NAME tst_ollama_parse COMMAND tst_ollama_parse)
set_tests_properties(tst_ollama_parse PROPERTIES LABELS "unit")

# 4. tst_openai_parse
add_executable(tst_openai_parse
    unit/tst_openai_parse.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/OpenAiIt.cpp"
)
target_link_libraries(tst_openai_parse PRIVATE margin_plugin_api Qt6::Core Qt6::Test)
target_include_directories(tst_openai_parse PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(tst_openai_parse PRIVATE FIXTURES_DIR="${_fixtures_dir}")
set_target_properties(tst_openai_parse PROPERTIES AUTOMOC ON)
add_test(NAME tst_openai_parse COMMAND tst_openai_parse)
set_tests_properties(tst_openai_parse PROPERTIES LABELS "unit")

# 5. tst_config
add_executable(tst_config
    unit/tst_config.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EngineConfig.cpp"
)
target_link_libraries(tst_config PRIVATE margin_plugin_api Qt6::Core Qt6::Test)
target_include_directories(tst_config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_config PROPERTIES AUTOMOC ON)
add_test(NAME tst_config COMMAND tst_config)
set_tests_properties(tst_config PROPERTIES LABELS "unit")

# 6. tst_emotion (23 个同构用例：状态机 + 健康分)
add_executable(tst_emotion
    unit/tst_emotion.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EmotionEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/Health.cpp"
)
target_link_libraries(tst_emotion PRIVATE margin_plugin_api Qt6::Core Qt6::Test)
target_include_directories(tst_emotion PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_emotion PROPERTIES AUTOMOC ON)
add_test(NAME tst_emotion COMMAND tst_emotion)
set_tests_properties(tst_emotion PROPERTIES LABELS "unit")

# 7. tst_telemetry_service
add_executable(tst_telemetry_service
    unit/tst_telemetry_service.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/TelemetryService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/LlamaCppIt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EmotionEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/Health.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EngineConfig.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/HalProvider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/NvmlHal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/SmiHal.cpp"
)
target_link_libraries(tst_telemetry_service PRIVATE margin_plugin_api Qt6::Core Qt6::Network Qt6::Test)
target_include_directories(tst_telemetry_service PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_telemetry_service PROPERTIES AUTOMOC ON)
add_test(NAME tst_telemetry_service COMMAND tst_telemetry_service)
set_tests_properties(tst_telemetry_service PROPERTIES LABELS "unit")

# 8. tst_dock_behavior
add_executable(tst_dock_behavior
    unit/tst_dock_behavior.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/DockBehavior.cpp"
)
target_link_libraries(tst_dock_behavior PRIVATE margin_plugin_api Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test)
target_include_directories(tst_dock_behavior PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_dock_behavior PROPERTIES AUTOMOC ON)
add_test(NAME tst_dock_behavior COMMAND tst_dock_behavior)
set_tests_properties(tst_dock_behavior PROPERTIES LABELS "unit")

# 9. tst_hotkey
add_executable(tst_hotkey
    unit/tst_hotkey.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/HotkeyManager.cpp"
)
target_link_libraries(tst_hotkey PRIVATE margin_plugin_api Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(tst_hotkey PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_hotkey PROPERTIES AUTOMOC ON)
add_test(NAME tst_hotkey COMMAND tst_hotkey)
set_tests_properties(tst_hotkey PROPERTIES LABELS "unit")

# 10. tst_loopback_guard
add_executable(tst_loopback_guard
    unit/tst_loopback_guard.cpp
)
target_link_libraries(tst_loopback_guard PRIVATE margin_plugin_api Qt6::Core Qt6::Test)
target_include_directories(tst_loopback_guard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_loopback_guard PROPERTIES AUTOMOC ON)
add_test(NAME tst_loopback_guard COMMAND tst_loopback_guard)
set_tests_properties(tst_loopback_guard PROPERTIES LABELS "unit")

# 11. tst_contributors
add_executable(tst_contributors
    unit/tst_contributors.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/LlamaPetPlugin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/TelemetryService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/DockBehavior.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/FloatingWindows.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/HotkeyManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../app/ClickThrough.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/LlamaCppIt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/OllamaIt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/OpenAiIt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/HalProvider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/NvmlHal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/SmiHal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EmotionEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/Health.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/EngineConfig.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../llamapet.qrc"
)
target_link_libraries(tst_contributors PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Test
)
target_include_directories(tst_contributors PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(tst_contributors PROPERTIES AUTOMOC ON)
add_test(NAME tst_contributors COMMAND tst_contributors)
set_tests_properties(tst_contributors PROPERTIES LABELS "unit")


