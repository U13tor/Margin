# src/paths/CMakeLists.txt
#
# Static library wrapping QStandardPaths. Linked into the host executable.
# Not exported to plugins (plugins receive paths via HostServices in M0-C5).

add_library(margin_paths STATIC
    Paths.cpp
)

target_include_directories(margin_paths PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core)
target_link_libraries(margin_paths PUBLIC Qt6::Core)
