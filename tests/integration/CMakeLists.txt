# Integration tests (Layer 2 per docs/09-testing.md §3). M0-C5b ships the
# plugin load/unload test against a minimal fake_plugin fixture DLL.

add_subdirectory(fake_plugin)
add_subdirectory(fake_plugin_b)

add_executable(test_plugin_load_unload test_plugin_load_unload.cpp)
target_link_libraries(test_plugin_load_unload PRIVATE
    margin_core          # PluginManager, HostServicesImpl
    margin_security      # CryptoServicePool (PRIVATE in margin_core, not transitive)
    margin_plugin_api    # Logger, EventBus, PluginInterface
    margin_fake          # fixture header include
    Qt6::Core
    Qt6::Test
    Qt6::Widgets         # margin_core pulls SystemTray (QSystemTrayIcon) transitively
)
target_include_directories(test_plugin_load_unload PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake_plugin"
    "${CMAKE_SOURCE_DIR}/src"   # for host/core/PluginManager.h, host/security/CryptoServicePool.h
)

# margin_fake_b is discovered at runtime (not linked), so force it to build
# before the test runs so margin-zlast.dll exists in test_plugins/.
add_dependencies(test_plugin_load_unload margin_fake_b)

add_test(NAME test_plugin_load_unload COMMAND test_plugin_load_unload)
set_tests_properties(test_plugin_load_unload PROPERTIES LABELS "integration")

# Inject the absolute path to the test-plugins directory so the test exe
# doesn't have to hardcode relative paths.
target_compile_definitions(test_plugin_load_unload PRIVATE
    MARGIN_TEST_PLUGINS_DIR="${CMAKE_BINARY_DIR}/test_plugins"
)
