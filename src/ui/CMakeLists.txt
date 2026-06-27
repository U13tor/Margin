# Margin main-panel UI (docs/06 §6).
#
# The reusable component-library modules (Margin.Ui.Primitives /
# Margin.Ui.Composite) are built here as STATIC qml modules. The app shell QML
# itself — DashboardWindow / TabBar / StatusBar — is compiled into the Margin
# executable via ui.qrc (added in src/host/CMakeLists.txt) and loaded by
# HostCore at qrc:/ui/DashboardWindow.qml. The shell imports
# Margin.Ui.Primitives for the Theme design tokens.

# Qml/Quick bring qt_add_qml_module into scope for the subdirectories below
# (this dir is configured before src/host, which has its own find_package).
find_package(Qt6 6.7 REQUIRED COMPONENTS Qml Quick)

# Adopt the modern ':/qt/qml/' default resource prefix for QML modules; the
# policy propagates to the module subdirectories added below (silences QTP0001).
if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()

add_subdirectory(primitives)   # Margin.Ui.Primitives
add_subdirectory(composite)    # Margin.Ui.Composite
add_subdirectory(charts)       # Margin.Ui.Charts
