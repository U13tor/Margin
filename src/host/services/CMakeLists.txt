# HostServices impl layer. Not exported to plugins (they see only margin_plugin_api).

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Qml Sql)

add_library(margin_services STATIC
    DatabaseImpl.cpp
    HostServicesImpl.cpp
    Logger.cpp
    Settings.cpp
)

target_link_libraries(margin_services
    PUBLIC margin_plugin_api
    PRIVATE Qt6::Core Qt6::Qml Qt6::Sql
           margin_security
)

target_include_directories(margin_services PRIVATE "${CMAKE_SOURCE_DIR}/src")
