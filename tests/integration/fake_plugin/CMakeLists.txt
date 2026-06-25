# Minimal test fixture plugin. Output goes to ${CMAKE_BINARY_DIR}/test_plugins/
# so tests/integration/test_plugin_load_unload.cpp can locate it via
# MARGIN_TEST_PLUGINS_DIR (set in the parent CMakeLists.txt).

add_library(margin_fake SHARED fake_plugin.cpp)

target_link_libraries(margin_fake PRIVATE
    margin_plugin_api
    Qt6::Core
)

# Produces margin-fake.dll / libmargin-fake.so / libmargin-fake.dylib.
set_target_properties(margin_fake PROPERTIES
    OUTPUT_NAME margin-fake
    PREFIX ""          # avoid 'lib' prefix on *nix
)

# Place the runtime binary (.dll/.so/.dylib) in a known plugins dir.
set_target_properties(margin_fake PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_plugins"
)

# Per-config output paths (multi-config generators like Visual Studio): the
# set_target_properties paths above get a per-config suffix appended; force
# them flat so the test can find the dll deterministically.
foreach(CFG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(margin_fake PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${CFG}" "${CMAKE_BINARY_DIR}/test_plugins"
        "RUNTIME_OUTPUT_DIRECTORY_${CFG}" "${CMAKE_BINARY_DIR}/test_plugins"
    )
endforeach()

# Manifest must sit beside the DLL as <basename>.manifest.json.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
    "${CMAKE_BINARY_DIR}/test_plugins/margin-fake.manifest.json"
    COPYONLY)
