# Second test fixture (priority 10). Output -> ${CMAKE_BINARY_DIR}/test_plugins
# alongside margin-fake so the load-order test discovers both.

add_library(margin_fake_b SHARED fake_plugin_b.cpp)

target_link_libraries(margin_fake_b PRIVATE
    margin_plugin_api
    Qt6::Core
)

# Produces margin-zlast.dll / libmargin-zlast.so / libmargin-zlast.dylib.
set_target_properties(margin_fake_b PROPERTIES
    OUTPUT_NAME margin-zlast
    PREFIX ""          # avoid 'lib' prefix on *nix
)

set_target_properties(margin_fake_b PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_plugins"
)

# Force flat output for multi-config generators (see fake_plugin/CMakeLists.txt).
foreach(CFG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(margin_fake_b PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${CFG}" "${CMAKE_BINARY_DIR}/test_plugins"
        "RUNTIME_OUTPUT_DIRECTORY_${CFG}" "${CMAKE_BINARY_DIR}/test_plugins"
    )
endforeach()

# Manifest must sit beside the DLL as <basename>.manifest.json.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
    "${CMAKE_BINARY_DIR}/test_plugins/margin-zlast.manifest.json"
    COPYONLY)
