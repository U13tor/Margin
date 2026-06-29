# Plugin SDK — stable ABI surface.
# Header-only for the abstract interfaces (Logger/Settings/TrayService/
# EventBus); STATIC archive so the EventBus.cpp impl (M0-C4) ships in the
# same target. Each interface maps 1:1 to docs/05-host-services.md.

find_package(Qt6 6.7 REQUIRED COMPONENTS Core)

add_library(margin_plugin_api STATIC
    src/EventBus.cpp
    # Listed here (not just as a header in include/) so AUTOMOC scans
    # WindowMonitorService.h / InputMonitorService.h's Q_OBJECT and emits
    # moc output into mocs_compilation.cpp. Both are QObject interfaces
    # with signals — without an entry in the target's sources, AUTOMOC
    # has nothing to scan.
    include/Margin/WindowMonitorService.h
    include/Margin/InputMonitorService.h
)

set_target_properties(margin_plugin_api PROPERTIES AUTOMOC ON)

target_include_directories(margin_plugin_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(margin_plugin_api PUBLIC Qt6::Core)

# 注入 host/plugin 共享的版本常量。
# - MARGIN_VERSION: 当前 Host API 版本(来自顶层 project()),用于 manifest 的
#   min_host_version 校验。 deliberately NOT tied to MARGIN_RELEASE_VERSION:
#   plugin compatibility is determined by source-level API surface, not by
#   git tag. Tying it to release tag would make dry-run builds (where
#   MARGIN_RELEASE_VERSION="dry-run") reject all plugins via
#   QVersionNumber::fromString returning null.
# - MARGIN_ABI_VERSION: PluginInterface ABI 版本,与 src/plugin_api/include/
#   Margin/AbiVersion.h 中的字面量保持一致;manifest 的 abi_version 字段必须
#   严格相等。详见 docs/04-plugin-spec.md §2 "版本协商"。
target_compile_definitions(margin_plugin_api PUBLIC
    MARGIN_VERSION="${CMAKE_PROJECT_VERSION}"
    MARGIN_ABI_VERSION="0.2.0"
)
