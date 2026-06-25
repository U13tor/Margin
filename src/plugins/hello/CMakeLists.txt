# Hello plugin (M0-C6) — minimal reference plugin.
# Spec: docs/04-plugin-spec.md §8 + docs/03-build-system.md.
#
# Output: margin-hello.dll / libmargin-hello.so / libmargin-hello.dylib,
# sitting next to Margin.exe under <build>/src/host/plugins/ so
# PluginManager::loadAll({Paths::officialPlugins(), ...}) picks it up at
# dev time (Paths::officialPlugins() = QCoreApplication::applicationDirPath()
# + "/plugins").

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(margin_hello SHARED
    entry.cpp
    HelloPlugin.cpp
)

target_link_libraries(margin_hello PRIVATE
    margin_plugin_api
    Qt6::Core
)

# Output name 'margin-hello' (CLAUDE.md §4 plugin binary naming convention).
# PREFIX "" avoids 'lib' on *nix (M0 ships Win+Mac only, but keep it portable).
set_target_properties(margin_hello PROPERTIES
    OUTPUT_NAME margin-hello
    PREFIX ""
)

# Copy dll + manifest next to Margin.exe/plugins/ so PluginManager finds it.
# copy_if_different keeps incremental builds fast.
set(_hello_plugins_dir "$<TARGET_FILE_DIR:Margin>/plugins")

add_custom_command(TARGET margin_hello POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_hello_plugins_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:margin_hello>"
        "${_hello_plugins_dir}/$<TARGET_FILE_NAME:margin_hello>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
        "${_hello_plugins_dir}/margin-hello.manifest.json"
)
