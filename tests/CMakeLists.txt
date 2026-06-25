# Tests — Qt Test + CTest integration. M0-C4 shipped the EventBus unit
# skeleton; M0-C5b adds integration tests (plugin load/unload). M0-C8 adds
# Keyring + per-plugin crypto tests. M0-C9 will expand to smoke + CI matrix.

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Test Widgets Sql)

# Layer 2: Unit tests (per docs/09-testing.md §3).
add_executable(test_event_bus unit/test_event_bus.cpp)
target_link_libraries(test_event_bus PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
add_test(NAME test_event_bus COMMAND test_event_bus)
set_tests_properties(test_event_bus PROPERTIES LABELS "unit")

# M2-C2: DatabaseImpl round-trip + transaction + WAL pragmas. Uses
# QTemporaryDir per test so the real margin.db is never touched.
add_executable(test_database unit/test_database.cpp)
target_link_libraries(test_database PRIVATE
    margin_services
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
target_include_directories(test_database PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME test_database COMMAND test_database)
set_tests_properties(test_database PROPERTIES LABELS "unit")

# M2-C1: ActiveWindowTrackerWin hook lifecycle. Real foreground switches
# need UI interaction (manual L4 test), so this unit test only covers
# start/stop/double-call safety + resolveForeground smoke. Windows-only.
# AUTOMOC required: tracker + test both use Q_OBJECT.
if(WIN32)
    add_executable(test_active_window_tracker unit/test_active_window_tracker.cpp)
    target_link_libraries(test_active_window_tracker PRIVATE
        margin_platform
        Qt6::Core
        Qt6::Test
    )
    target_include_directories(test_active_window_tracker PRIVATE "${CMAKE_SOURCE_DIR}/src")
    set_target_properties(test_active_window_tracker PROPERTIES AUTOMOC ON)
    add_test(NAME test_active_window_tracker COMMAND test_active_window_tracker)
    set_tests_properties(test_active_window_tracker PROPERTIES LABELS "unit")
endif()

# M2-C4: IdleDetectorWin hook lifecycle + timer-driven idle state machine.
# Real keyboard/mouse input can't be synthesized on CI runners, so the
# unit test covers start/stop + threshold getter/setter + double-call
# safety + the natural timer firing (no input → idle signal). The
# input-cancel-idle edge is an L4 manual test. Windows-only. AUTOMOC
# required: IdleDetectorWin + test both use Q_OBJECT.
if(WIN32)
    add_executable(test_idle_detector unit/test_idle_detector.cpp)
    target_link_libraries(test_idle_detector PRIVATE
        margin_platform
        Qt6::Core
        Qt6::Test
    )
    target_include_directories(test_idle_detector PRIVATE "${CMAKE_SOURCE_DIR}/src")
    set_target_properties(test_idle_detector PROPERTIES AUTOMOC ON)
    add_test(NAME test_idle_detector COMMAND test_idle_detector)
    set_tests_properties(test_idle_detector PROPERTIES LABELS "unit")
endif()

# M4-C10 / M0-C10c: PlatformBackend::applyDarkTitleBar defensive contract.
# Windows-only — Mac impl deferred per docs/12 §A15. QTEST_MAIN selects
# QGuiApplication (no Widgets linked) which is what QWindow requires.
# AUTOMOC required: test class has Q_OBJECT.
if(WIN32)
    add_executable(test_dark_title_bar unit/test_dark_title_bar.cpp)
    target_link_libraries(test_dark_title_bar PRIVATE
        margin_platform
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    target_include_directories(test_dark_title_bar PRIVATE "${CMAKE_SOURCE_DIR}/src")
    set_target_properties(test_dark_title_bar PROPERTIES AUTOMOC ON)
    add_test(NAME test_dark_title_bar COMMAND test_dark_title_bar)
    set_tests_properties(test_dark_title_bar PROPERTIES LABELS "unit")
endif()

# Windows autostart (HKCU\...\Run\Margin round-trip). Windows-only —
# Mac/Linux backend is nullptr; the suite QSKIPs on those platforms.
# QTEST_GUILESS_MAIN: registry calls don't need a GUI thread, but
# QCoreApplication is required for QCoreApplication::applicationFilePath()
# (AUTOMOC handles moc generation).
if(WIN32)
    add_executable(test_autostart_win unit/test_autostart_win.cpp)
    target_link_libraries(test_autostart_win PRIVATE
        margin_platform
        Qt6::Core
        Qt6::Test
    )
    target_include_directories(test_autostart_win PRIVATE "${CMAKE_SOURCE_DIR}/src")
    set_target_properties(test_autostart_win PROPERTIES AUTOMOC ON)
    add_test(NAME test_autostart_win COMMAND test_autostart_win)
    set_tests_properties(test_autostart_win PROPERTIES LABELS "unit")
endif()

# M0-C8: per-plugin crypto service (HKDF + AES-256-GCM via OpenSSL). Runs on
# all platforms — does not touch the OS keyring.
add_executable(test_crypto_service unit/test_crypto_service.cpp)
target_link_libraries(test_crypto_service PRIVATE
    margin_security
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_crypto_service PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME test_crypto_service COMMAND test_crypto_service)
set_tests_properties(test_crypto_service PROPERTIES LABELS "unit")

# M0-C8: OS keyring KV primitive (Win DPAPI). Mac is gated by C6 (Keychain
# implementation deferred, see docs/12-deferred-items.md) — branches qFatal
# at runtime so we skip registration on non-Win.
if(WIN32)
    add_executable(test_keyring unit/test_keyring.cpp)
    target_link_libraries(test_keyring PRIVATE
        margin_security
        Qt6::Core
        Qt6::Test
    )
    target_include_directories(test_keyring PRIVATE "${CMAKE_SOURCE_DIR}/src")
    add_test(NAME test_keyring COMMAND test_keyring)
    set_tests_properties(test_keyring PROPERTIES LABELS "unit")
endif()

# M0-C10a/b: dashboard shell QML load test. Runs offscreen (set via test
# ENVIRONMENT) so it is headless on CI. Links the Primitives qml-module plugin
# so the shell's `import Margin.Ui.Primitives` resolves; ui.qrc provides the
# qrc:/ui/* shell. Links margin_core so the test can drive the real
# DashboardTabRegistry (M0-C10b: Tab host + plugin contributor registry).
# No Widgets → QTEST_MAIN uses QGuiApplication.
add_executable(test_dashboard_shell
    unit/test_dashboard_shell.cpp
    "${CMAKE_SOURCE_DIR}/src/ui/ui.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_dashboard_shell PRIVATE
    margin_core
    margin_ui_compositeplugin
    margin_ui_primitivesplugin
    margin_ui_chartsplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
target_include_directories(test_dashboard_shell PRIVATE "${CMAKE_SOURCE_DIR}/src")
qt_import_qml_plugins(test_dashboard_shell)
add_test(NAME test_dashboard_shell COMMAND test_dashboard_shell)
set_tests_properties(test_dashboard_shell PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C1: Theme design-token singleton contract. Loads an inline QML probe that
# imports Margin.Ui.Primitives and reads Theme.* back into C++ — guards the full
# §2 token set (type scale / spacing / radius / motion / font stacks) that the
# shell-load test (colours + version only) doesn't cover. Mirrors
# test_dashboard_shell: links the Primitives qml-module plugin so the import
# resolves, runs offscreen. Global CMAKE_AUTOMOC handles the .moc.
add_executable(test_theme_tokens unit/test_theme_tokens.cpp)
target_link_libraries(test_theme_tokens PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_theme_tokens)
add_test(NAME test_theme_tokens COMMAND test_theme_tokens)
set_tests_properties(test_theme_tokens PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C2: bundled font registration. Verifies Inter (Regular/Medium/SemiBold) +
# JetBrains Mono (Regular/Medium) — fetched at configure time by
# cmake/fetch_fonts.cmake and compiled in via fonts.qrc — register under the
# expected family names. Mirrors test_dashboard_shell: links fonts.qrc directly.
# QTEST_MAIN picks QGuiApplication because Qt6::Gui is linked (QFontDatabase
# needs it). If the fetch script couldn't reach the network, fonts.qrc is empty
# and the test self-skips via QSKIP in initTestCase — only the bogus-path
# -1-return assertion runs in that mode.
add_executable(test_font_registration
    unit/test_font_registration.cpp
    ${MARGIN_FONTS_QRC}
)
target_link_libraries(test_font_registration PRIVATE
    Qt6::Gui
    Qt6::Test
)
add_test(NAME test_font_registration COMMAND test_font_registration)
set_tests_properties(test_font_registration PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C3: MIcon atom contract — Image+MultiEffect tinting. Mirrors
# test_theme_tokens: inline QML probe + property read-back. Links the
# Primitives qml-module plugin so `import Margin.Ui.Primitives` resolves +
# host.qrc so qrc:/icons/icon-bell.svg resolves. Runs offscreen.
add_executable(test_micon
    unit/test_micon.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_micon PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_micon)
add_test(NAME test_micon COMMAND test_micon)
set_tests_properties(test_micon PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C11: StatusBar.qml rewrite — mode dot + mono duration + 3 link hooks.
# Mirrors test_micon/test_overview_tab: stub rhythm QObject + context
# property + offscreen QML probe. Links the Primitives qml-module plugin
# (Theme + MIcon) + ui.qrc (StatusBar.qml itself) + host.qrc (icons).
# AUTOMOC required: RhythmStub has Q_OBJECT.
add_executable(test_status_bar
    unit/test_status_bar.cpp
    "${CMAKE_SOURCE_DIR}/src/ui/ui.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_status_bar PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
target_include_directories(test_status_bar PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_status_bar PROPERTIES AUTOMOC ON)
qt_import_qml_plugins(test_status_bar)
add_test(NAME test_status_bar COMMAND test_status_bar)
set_tests_properties(test_status_bar PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C4: MButton atom contract — variant color matrix (Primary/Secondary/
# Ghost) + icon slot + clicked() signal. Same probe pattern as test_micon.
add_executable(test_mbutton
    unit/test_mbutton.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_mbutton PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_mbutton)
add_test(NAME test_mbutton COMMAND test_mbutton)
set_tests_properties(test_mbutton PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C5: MSwitch atom contract — checked property + derived track/thumb
# colors + toggled(bool) signal. Same probe pattern as test_mbutton.
add_executable(test_mswitch
    unit/test_mswitch.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_mswitch PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_mswitch)
add_test(NAME test_mswitch COMMAND test_mswitch)
set_tests_properties(test_mswitch PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C5: MSlider atom contract — from/to/value/stepSize defaults + clamp +
# step snap + moved(real) signal. Drag is exercised through _setValueFromFraction.
add_executable(test_mslider
    unit/test_mslider.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_mslider PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_mslider)
add_test(NAME test_mslider COMMAND test_mslider)
set_tests_properties(test_mslider PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C6: MCard atom contract — default padding/radius + override + default
# children slot population.
add_executable(test_mcard
    unit/test_mcard.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_mcard PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_mcard)
add_test(NAME test_mcard COMMAND test_mcard)
set_tests_properties(test_mcard PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C6: MListItem atom contract — title/subtitle/iconSource + derived
# _hasIcon/_hasSubtitle flags + default trailing slot population.
add_executable(test_mlistitem
    unit/test_mlistitem.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_mlistitem PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_mlistitem)
add_test(NAME test_mlistitem COMMAND test_mlistitem)
set_tests_properties(test_mlistitem PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C7: StatusCard composite contract — title/value/subtitle propagation,
# icon slot hide/show, default extras slot population, clicked signal.
# Links both primitives + composite qml-module plugins (StatusCard composes
# MCard + MIcon atoms, so Margin.Ui.Primitives must also resolve).
add_executable(test_statuscard
    unit/test_statuscard.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_statuscard PRIVATE
    margin_ui_compositeplugin
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_statuscard)
add_test(NAME test_statuscard COMMAND test_statuscard)
set_tests_properties(test_statuscard PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C7: TimerDisplay composite contract — MmSs / HhMmSs formatting,
# negative-seconds clamp, text override, label propagation, Format default.
add_executable(test_timerdisplay
    unit/test_timerdisplay.cpp
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_timerdisplay PRIVATE
    margin_ui_compositeplugin
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_timerdisplay)
add_test(NAME test_timerdisplay COMMAND test_timerdisplay)
set_tests_properties(test_timerdisplay PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C8: OverviewTab.qml rewrite verification. Loads DashboardWindow.qml with
# a real DashboardTabRegistry + stub plugin context properties and verifies
# the 3 StatusCards bind correctly + typeof guards + subtitle hide-when-dash
# + click-to-tab navigation. Mirrors test_dashboard_shell: links margin_core
# (for DashboardTabRegistry), the Primitives + Composite qml-module plugins
# (so StatusCard + MCard + MIcon resolve), and ui.qrc + host.qrc so the
# shell + icons load. AUTOMOC required: stub QObjects have Q_OBJECT in the
# .cpp, trailing .moc include resolves.
add_executable(test_overview_tab
    unit/test_overview_tab.cpp
    "${CMAKE_SOURCE_DIR}/src/ui/ui.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_overview_tab PRIVATE
    margin_core
    margin_ui_compositeplugin
    margin_ui_primitivesplugin
    margin_ui_chartsplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
target_include_directories(test_overview_tab PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_overview_tab PROPERTIES AUTOMOC ON)
qt_import_qml_plugins(test_overview_tab)
add_test(NAME test_overview_tab COMMAND test_overview_tab)
set_tests_properties(test_overview_tab PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M0-C10b: DashboardTabRegistry + DashboardTabContributor wiring. Pure C++
# (no QML load); exercises the host registry's addTab / sortByOrder round-trip
# and a fake plugin's asDashboardTab() / tabInfo() contract.
add_executable(test_dashboard_tab_contributor
    unit/test_dashboard_tab_contributor.cpp
)
target_link_libraries(test_dashboard_tab_contributor PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_dashboard_tab_contributor PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME test_dashboard_tab_contributor COMMAND test_dashboard_tab_contributor)
set_tests_properties(test_dashboard_tab_contributor PROPERTIES LABELS "unit")

# M5-C2: SettingsRegistry + SettingsPageContributor wiring. Mirrors
# test_dashboard_tab_contributor's shape; additionally verifies the `section`
# field survives QVariantMap serialization (sidebar grouping depends on it).
add_executable(test_settings_registry
    unit/test_settings_registry.cpp
)
target_link_libraries(test_settings_registry PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_settings_registry PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_settings_registry PROPERTIES AUTOMOC ON)
add_test(NAME test_settings_registry COMMAND test_settings_registry)
set_tests_properties(test_settings_registry PROPERTIES LABELS "unit")

# M5-C4e: SettingsPageContributor contract — PageInfo round-trip + section
# grouping (mirrors test_dashboard_tab_contributor). AUTOMOC for Q_OBJECT.
add_executable(test_settings_page_contributor
    unit/test_settings_page_contributor.cpp
)
target_link_libraries(test_settings_page_contributor PRIVATE
    margin_plugin_api
    margin_core
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_settings_page_contributor PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_settings_page_contributor PROPERTIES AUTOMOC ON)
add_test(NAME test_settings_page_contributor COMMAND test_settings_page_contributor)
set_tests_properties(test_settings_page_contributor PROPERTIES LABELS "unit")

# M4-C9b.1: ActivityFeed contract — curated-topic subscription, ring-buffer
# cap, signal wiring, destruction-time unsubscribe. PR6 adds cross-restart
# persistence (host_activity_events), overflow row-trimming in DB, and
# graceful degradation when no Database is attached. DatabaseImpl.cpp is
# compiled in directly (mirrors test_session_store) so the test can drive a
# temp SQLite file without linking the full host DLL.
add_executable(test_activity_feed
    unit/test_activity_feed.cpp
    "${CMAKE_SOURCE_DIR}/src/host/services/DatabaseImpl.cpp"
)
target_link_libraries(test_activity_feed PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
target_include_directories(test_activity_feed PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_activity_feed PROPERTIES AUTOMOC ON)
add_test(NAME test_activity_feed COMMAND test_activity_feed)
set_tests_properties(test_activity_feed PROPERTIES LABELS "unit")

# M1-C3: RssiSmoother sliding-window RSSI averager. Pure algorithm — no
# Qt signals, no BLE hardware. Source is included directly here for now;
# M1-C5 moves RssiSmoother into the margin-aura-locker plugin DLL.
add_executable(test_rssi_smoother
    unit/test_rssi_smoother.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/RssiSmoother.cpp"
)
target_link_libraries(test_rssi_smoother PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_rssi_smoother PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME test_rssi_smoother COMMAND test_rssi_smoother)
set_tests_properties(test_rssi_smoother PROPERTIES LABELS "unit")

# M1-C4: PlatformBackend factory + LockService contract. Verifies the host
# can produce a LockService on Win and that the returned backend IS-A
# LockService (inheritance wiring). Does NOT call real LockWorkStation —
# that would lock the dev/CI machine.
add_executable(test_lock_service unit/test_lock_service.cpp)
target_link_libraries(test_lock_service PRIVATE
    margin_platform
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_lock_service PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME test_lock_service COMMAND test_lock_service)
set_tests_properties(test_lock_service PROPERTIES LABELS "unit")

# M1-C1+C2: BluetoothProximityTracker interface + factory. FakeTracker
# subclass drives the signal contract through QSignalSpy; on Win the
# factory must return a non-null tracker (BluetoothProximityTrackerWin).
# Does NOT start real BLE hardware — that lives in tests/manual/M1_ACCEPTANCE.md.
# AUTOMOC is required: FakeTracker has a Q_OBJECT inside the .cpp, so the
# trailing `#include "test_bluetooth_proximity_tracker.moc"` resolves.
#
# Sources are compiled directly into the test exe (not linked from the
# plugin DLL) because plugins don't dllexport their C++ symbols — there's
# no Q_DECL_EXPORT on BluetoothProximityTracker. This mirrors the
# test_rssi_smoother pattern.
add_executable(test_bluetooth_proximity_tracker
    unit/test_bluetooth_proximity_tracker.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/BluetoothProximityTracker.cpp"
)
if(WIN32)
    target_sources(test_bluetooth_proximity_tracker PRIVATE
        "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/win/BluetoothProximityTrackerWin.cpp"
    )
    target_link_libraries(test_bluetooth_proximity_tracker PRIVATE margin_cppwinrt_iface)
endif()
target_link_libraries(test_bluetooth_proximity_tracker PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_bluetooth_proximity_tracker PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_bluetooth_proximity_tracker PROPERTIES AUTOMOC ON)
add_test(NAME test_bluetooth_proximity_tracker COMMAND test_bluetooth_proximity_tracker)
set_tests_properties(test_bluetooth_proximity_tracker PROPERTIES LABELS "unit")

# M1 integration: ProximityDetector state machine — pure logic, no BLE
# hardware. Synthetic RSSI samples + QSignalSpy drive the away → lock →
# cooldown round-trip. Source compiled directly into the test exe (plugins
# don't dllexport their C++ symbols). AUTOMOC required for Q_OBJECT in .cpp.
add_executable(test_proximity_detector
    unit/test_proximity_detector.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/ProximityDetector.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/RssiSmoother.cpp"
)
target_link_libraries(test_proximity_detector PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_proximity_detector PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_proximity_detector PROPERTIES AUTOMOC ON)
add_test(NAME test_proximity_detector COMMAND test_proximity_detector)
set_tests_properties(test_proximity_detector PROPERTIES LABELS "unit")

# M1-C6a: QmlServiceImpl minimal impl — context property registration,
# engine() accessor, registerType stub. Uses a hermetic QQmlEngine (no
# DashboardWindow load). AUTOMOC required: FakeStateObject has Q_OBJECT
# in the test .cpp, so the trailing .moc include resolves.
add_executable(test_qml_service unit/test_qml_service.cpp)
target_link_libraries(test_qml_service PRIVATE
    margin_services
    margin_plugin_api
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)
target_include_directories(test_qml_service PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_qml_service PROPERTIES AUTOMOC ON)
add_test(NAME test_qml_service COMMAND test_qml_service)
set_tests_properties(test_qml_service PROPERTIES LABELS "unit")

# M1-C7: AuraLockerPlugin BT-off safety guard + encrypted settings
# verification. The safety test drives a FakeTracker + real
# ProximityDetector through the lockRequested path with the BT radio
# off — verifies m_canLock suppression survives the away window.
# The encrypted-settings test reads back the raw settings.json file
# to confirm AES-GCM envelopes (not plaintext MACs) hit disk.
# AUTOMOC required for Q_OBJECT in test .cpp (FakeTracker in the
# safety test).
add_executable(test_aura_safety
    unit/test_aura_safety.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/BluetoothProximityTracker.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/ProximityDetector.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/RssiSmoother.cpp"
)
if(WIN32)
    target_sources(test_aura_safety PRIVATE
        "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/win/BluetoothProximityTrackerWin.cpp"
    )
    target_link_libraries(test_aura_safety PRIVATE margin_cppwinrt_iface)
endif()
target_link_libraries(test_aura_safety PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)
target_include_directories(test_aura_safety PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_aura_safety PROPERTIES AUTOMOC ON)
add_test(NAME test_aura_safety COMMAND test_aura_safety)
set_tests_properties(test_aura_safety PROPERTIES LABELS "unit")

# Encrypted-settings test links the real SettingsImpl + Keyring so it
# can prove the on-disk envelope is AES-GCM ciphertext rather than
# plaintext. Compiled directly into the test exe (not via margin_core)
# so failures localize to Settings.cpp.
add_executable(test_aura_encrypted_settings
    unit/test_aura_encrypted_settings.cpp
    "${CMAKE_SOURCE_DIR}/src/host/services/Settings.cpp"
)
target_link_libraries(test_aura_encrypted_settings PRIVATE
    margin_security
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_aura_encrypted_settings PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME test_aura_encrypted_settings COMMAND test_aura_encrypted_settings)
set_tests_properties(test_aura_encrypted_settings PROPERTIES LABELS "unit")

# A1 regression: every EventBus topic publish()'d in AuraLockerPlugin.cpp
# must be declared in manifest.json events.publish (CLAUDE.md §7 — no
# undeclared extension points). The test loads the source-tree manifest
# via AURA_MANIFEST_PATH and asserts the 4 expected topics are present.
add_executable(test_aura_manifest_events
    unit/test_aura_manifest_events.cpp
)
target_link_libraries(test_aura_manifest_events PRIVATE
    Qt6::Core
    Qt6::Test
)
target_compile_definitions(test_aura_manifest_events PRIVATE
    AURA_MANIFEST_PATH="${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/manifest.json"
)
add_test(NAME test_aura_manifest_events COMMAND test_aura_manifest_events)
set_tests_properties(test_aura_manifest_events PROPERTIES LABELS "unit")

# A3 ghost-device eviction: pruneStaleScanned drops entries whose
# last-seen timestamp is older than kScanStaleMs. Pure-algorithm test
# against the static helper; no HostServices / tracker instantiation
# needed. Mirrors test_rssi_smoother's direct-compile pattern.
add_executable(test_aura_scan_stale_eviction
    unit/test_aura_scan_stale_eviction.cpp
)
target_link_libraries(test_aura_scan_stale_eviction PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_aura_scan_stale_eviction PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_aura_scan_stale_eviction PROPERTIES AUTOMOC ON)
add_test(NAME test_aura_scan_stale_eviction COMMAND test_aura_scan_stale_eviction)
set_tests_properties(test_aura_scan_stale_eviction PROPERTIES LABELS "unit")

# PR7: AuraLockerPlugin recent-activity persistence — cross-restart replay,
# 30-row ring buffer cap at the DB layer (not just in-memory), and graceful
# degradation when no Database is attached. Compiles AuraLockerPlugin.cpp +
# its tracker/detector dependencies directly into the test exe, mirroring
# test_aura_safety: plugins are SHARED DLLs without dllexport on C++ symbols,
# so direct compile-in is the only way for the test to reach the private
# ensureRecentEventSchema / loadRecentEventsFromDb via the friend seam.
# DatabaseImpl.cpp is also compiled in (test_session_store pattern) so each
# test owns a private temp SQLite file.
add_executable(test_aura_recent_events_persist
    unit/test_aura_recent_events_persist.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/AuraLockerPlugin.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/BluetoothProximityTracker.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/ProximityDetector.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/RssiSmoother.cpp"
    "${CMAKE_SOURCE_DIR}/src/host/services/DatabaseImpl.cpp"
)
if(WIN32)
    target_sources(test_aura_recent_events_persist PRIVATE
        "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/win/BluetoothProximityTrackerWin.cpp"
    )
    target_link_libraries(test_aura_recent_events_persist PRIVATE margin_cppwinrt_iface)
endif()
target_link_libraries(test_aura_recent_events_persist PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Core
    Qt6::Qml
    Qt6::Sql
    Qt6::Test
)
target_include_directories(test_aura_recent_events_persist PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker"
)
set_target_properties(test_aura_recent_events_persist PROPERTIES AUTOMOC ON)
add_test(NAME test_aura_recent_events_persist COMMAND test_aura_recent_events_persist)
set_tests_properties(test_aura_recent_events_persist PROPERTIES LABELS "unit")

# M2-C3: SessionStore schema + open/close/pickup round-trip + time-bucket
# math. Compiled directly into the test exe (mirrors test_rssi_smoother:
# the screen_time plugin is a SHARED DLL without dllexport on SessionStore).
# Uses a temp-file DatabaseImpl so the real margin.db is untouched. Links
# margin_plugin_api for the Margin/Database.h interface headers.
add_executable(test_session_store
    unit/test_session_store.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/screen_time/SessionStore.cpp"
    "${CMAKE_SOURCE_DIR}/src/host/services/DatabaseImpl.cpp"
)
target_link_libraries(test_session_store PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
target_include_directories(test_session_store PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_session_store PROPERTIES AUTOMOC ON)
add_test(NAME test_session_store COMMAND test_session_store)
set_tests_properties(test_session_store PROPERTIES LABELS "unit")

# M2-C5: CategoryMatcher rule loading + matching + user-override
# precedence + bad-regex resilience. Compiled directly into the test
# exe (mirrors test_session_store: the plugin is SHARED, doesn't
# dllexport C++ symbols). The screen_time.qrc is linked so the test
# can read qrc:/screen_time/default_categories.json just like the
# plugin does.
add_executable(test_category_matcher
    unit/test_category_matcher.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/screen_time/CategoryMatcher.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/screen_time/screen_time.qrc"
)
target_link_libraries(test_category_matcher PRIVATE
    margin_services
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_category_matcher PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_category_matcher PROPERTIES AUTOMOC ON)
add_test(NAME test_category_matcher COMMAND test_category_matcher)
set_tests_properties(test_category_matcher PROPERTIES LABELS "unit")

# B2: DashboardTabRegistry pre-builds its QVariantList cache once per
# mutation (addTab / sortByOrder) so QML ListView reads don't rebuild
# on every refresh.
add_executable(test_dashboard_registry_cache
    unit/test_dashboard_registry_cache.cpp
)
target_link_libraries(test_dashboard_registry_cache PRIVATE
    margin_core
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_dashboard_registry_cache PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_dashboard_registry_cache PROPERTIES AUTOMOC ON)
add_test(NAME test_dashboard_registry_cache COMMAND test_dashboard_registry_cache)
set_tests_properties(test_dashboard_registry_cache PROPERTIES LABELS "unit")

# B5: setter clamp regression. The static clamp* helpers are the SSOT
# for floor / ceiling math — both onLoad (settings-load) and the Q_PROPERTY
# setters route through them, so a hand-edited settings.json with
# cooldown_seconds=5 can't bypass the floor.
add_executable(test_aura_settings_clamp
    unit/test_aura_settings_clamp.cpp
)
target_link_libraries(test_aura_settings_clamp PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_aura_settings_clamp PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_aura_settings_clamp PROPERTIES AUTOMOC ON)
add_test(NAME test_aura_settings_clamp COMMAND test_aura_settings_clamp)
set_tests_properties(test_aura_settings_clamp PROPERTIES LABELS "unit")

# M3-C1: PomodoroTimer state machine — pure logic, no HostServices / no BLE.
# Mirrors test_rssi_smoother / test_proximity_detector pattern: source
# compiled directly into the test exe (the rhythm plugin is SHARED, doesn't
# dllexport C++ symbols). AUTOMOC required: PomodoroTimer has Q_OBJECT.
add_executable(test_pomodoro_timer
    unit/test_pomodoro_timer.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/rhythm/PomodoroTimer.cpp"
)
target_link_libraries(test_pomodoro_timer PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_pomodoro_timer PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_pomodoro_timer PROPERTIES AUTOMOC ON)
add_test(NAME test_pomodoro_timer COMMAND test_pomodoro_timer)
set_tests_properties(test_pomodoro_timer PROPERTIES LABELS "unit")

# M3-C4: OverlayRegistry polls contributors every 500ms and caches a
# QVariantList of { overlayQml } for OverlayContainer.qml. The cache + the
# "only emit when set changes" invariant are the load-bearing assertions —
# a noisy emit-per-poll would cause QML Repeater churn.
add_executable(test_overlay_registry
    unit/test_overlay_registry.cpp
)
target_link_libraries(test_overlay_registry PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_overlay_registry PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_overlay_registry PROPERTIES AUTOMOC ON)
add_test(NAME test_overlay_registry COMMAND test_overlay_registry)
set_tests_properties(test_overlay_registry PROPERTIES LABELS "unit")

# M3-C5: Aura away/back EventBus integration. The plugin subscribes the
# same shape from onLoad; this test exercises the path with a real EventBus
# instance (no QmlService / no InputMonitor needed). Compiles the timer
# source directly (no plugin dllexport).
add_executable(test_rhythm_aura_integration
    unit/test_rhythm_aura_integration.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/rhythm/PomodoroTimer.cpp"
)
target_link_libraries(test_rhythm_aura_integration PRIVATE
    margin_plugin_api
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_rhythm_aura_integration PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_rhythm_aura_integration PROPERTIES AUTOMOC ON)
add_test(NAME test_rhythm_aura_integration COMMAND test_rhythm_aura_integration)
set_tests_properties(test_rhythm_aura_integration PROPERTIES LABELS "unit")

# M4-C12: RhythmTab.qml pomodoro-fidelity rewrite. Loads the tab offscreen
# with a real PomodoroTimer instance as `rhythm` context property. Compiles
# PomodoroTimer.cpp directly (no plugin dllexport) and links Primitives +
# Composite qml-module plugins so Theme/MCard/MButton/TimerDisplay/
# ProgressDots resolve. rhythm.qrc + host.qrc registered as sources so
# qrc:/rhythm/... and qrc:/icons/... URLs work.
add_executable(test_rhythm_tab
    unit/test_rhythm_tab.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/rhythm/PomodoroTimer.cpp"
    "${CMAKE_SOURCE_DIR}/src/plugins/rhythm/rhythm.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_rhythm_tab PRIVATE
    margin_ui_compositeplugin
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
target_include_directories(test_rhythm_tab PRIVATE "${CMAKE_SOURCE_DIR}/src")
qt_import_qml_plugins(test_rhythm_tab)
set_target_properties(test_rhythm_tab PROPERTIES AUTOMOC ON)
add_test(NAME test_rhythm_tab COMMAND test_rhythm_tab)
set_tests_properties(test_rhythm_tab PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M3-C6: Rhythm manifest/code consistency (mirrors A17 for Aura). Verifies
# every topic publish()'d or subscribe()'d in RhythmPlugin.cpp is declared
# in manifest.json, and that the 3 permissions + 3 ui_contributions match
# what the plugin actually overrides. Manifest path baked in via
# target_compile_definitions so the test reads the source tree, not the
# build tree.
add_executable(test_rhythm_manifest_events
    unit/test_rhythm_manifest_events.cpp
)
target_link_libraries(test_rhythm_manifest_events PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_rhythm_manifest_events PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(test_rhythm_manifest_events PRIVATE
    RHYTHM_MANIFEST_PATH="${CMAKE_SOURCE_DIR}/src/plugins/rhythm/manifest.json"
)
set_target_properties(test_rhythm_manifest_events PROPERTIES AUTOMOC ON)
add_test(NAME test_rhythm_manifest_events COMMAND test_rhythm_manifest_events)
set_tests_properties(test_rhythm_manifest_events PROPERTIES LABELS "unit")

# M4-C13b: NeckStretchAnimator QML surface + 8 PNG resource bundle.
# Verifies all 8 neck-stretch frames load from qrc, and that
# NeckStretchAnimator exposes currentStep / currentName / currentImage
# properties that BreakOverlay binds against. Mirrors test_rhythm_tab
# linking shape: rhythm.qrc + host.qrc as sources (for qrc URLs),
# primitives plugin for Theme (NeckStretchAnimator itself imports only
# QtQuick, but instantiating under the same engine as BreakOverlay needs
# the plugin loaded).
add_executable(test_neck_stretch_animator
    unit/test_neck_stretch_animator.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/rhythm/rhythm.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_neck_stretch_animator PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_neck_stretch_animator)
set_target_properties(test_neck_stretch_animator PROPERTIES AUTOMOC ON)
add_test(NAME test_neck_stretch_animator COMMAND test_neck_stretch_animator)
set_tests_properties(test_neck_stretch_animator PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C14a: AuraTab.qml fidelity-pass verification. Loads the tab offscreen
# with an AuraStub as `aura` context property (mirrors AuraLockerPlugin's
# onLoad registration). Verifies status-card state dot color flips with
# proximityState + event log ListView renders MListItem delegates + the
# Settings MButton pushes AuraSettingsPage onto the StackView. Mirrors
# test_rhythm_tab linking shape: aura_locker.qrc + host.qrc as sources
# (for qrc URLs), Primitives plugin for Theme + MCard + MButton + MListItem.
# AUTOMOC required: AuraStub has Q_OBJECT in the .cpp, trailing .moc include
# resolves.
add_executable(test_aura_tab
    unit/test_aura_tab.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/aura_locker.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_aura_tab PRIVATE
    margin_ui_primitivesplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_aura_tab)
set_target_properties(test_aura_tab PROPERTIES AUTOMOC ON)
add_test(NAME test_aura_tab COMMAND test_aura_tab)
set_tests_properties(test_aura_tab PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)

# M4-C15: ScreenTimeTab.qml + ExportClearDialog.qml controls migration.
# Loads the tab offscreen with a ScreenTimeStub as `screen_time` context
# property (mirrors ScreenTimePlugin::onLoad's QmlService registration).
# Verifies header MButtons (3 toggle + refresh + data menu) + dialog body
# MButtons (export JSON/CSV + clear all + close) + signal wiring for
# refresh / dialog open / close / clear-guard. Mirrors test_aura_tab
# linking shape: screen_time.qrc + host.qrc as sources (for qrc URLs),
# Primitives + Charts plugins (ScreenTimeTab instantiates MBarChart +
# MDonutChart). AUTOMOC required: ScreenTimeStub has Q_OBJECT in the .cpp,
# trailing .moc include resolves.
add_executable(test_screen_time_tab
    unit/test_screen_time_tab.cpp
    "${CMAKE_SOURCE_DIR}/src/plugins/screen_time/screen_time.qrc"
    "${CMAKE_SOURCE_DIR}/src/resources/host.qrc"
)
target_link_libraries(test_screen_time_tab PRIVATE
    margin_ui_primitivesplugin
    margin_ui_chartsplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)
qt_import_qml_plugins(test_screen_time_tab)
set_target_properties(test_screen_time_tab PROPERTIES AUTOMOC ON)
add_test(NAME test_screen_time_tab COMMAND test_screen_time_tab)
set_tests_properties(test_screen_time_tab PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)

# Layer 3: Integration tests.
# M4-C16: SystemTray 5-section structure + refresh-on-demand. The tray's
# QSystemTrayIcon is constructed but never shown — the test inspects the
# QMenu actions via menuForTesting(). Links margin_core for SystemTray +
# AboutDialog. AUTOMOC required: Q_OBJECT in the test .cpp.
add_executable(test_system_tray unit/test_system_tray.cpp)
target_link_libraries(test_system_tray PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_system_tray PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_system_tray PROPERTIES AUTOMOC ON)
add_test(NAME test_system_tray COMMAND test_system_tray)
set_tests_properties(test_system_tray PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# PR3 round-2 #2b: AppIconProvider contract — cache lookup, empty-path
# fallback, missing-path fallback, notepad.exe round-trip (Win-only).
# Links margin_core for AppIconProvider + Qt6::Quick for QQuickImageProvider
# (AppIconProvider.h includes the header) + Qt6::Gui for QImage / QPixmap.
# AUTOMOC required: Q_OBJECT in the test .cpp.
add_executable(test_app_icon_provider unit/test_app_icon_provider.cpp)
target_link_libraries(test_app_icon_provider PRIVATE
    margin_core
    Qt6::Gui
    Qt6::Quick
    Qt6::Test
)
target_include_directories(test_app_icon_provider PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_app_icon_provider PROPERTIES AUTOMOC ON)
add_test(NAME test_app_icon_provider COMMAND test_app_icon_provider)
set_tests_properties(test_app_icon_provider PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# M4-C16: AboutDialog content contract — version from MARGIN_VERSION,
# license note, app name. Pure widget inspection (no show()).
add_executable(test_about_dialog unit/test_about_dialog.cpp)
target_link_libraries(test_about_dialog PRIVATE
    margin_core
    margin_plugin_api
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_about_dialog PRIVATE "${CMAKE_SOURCE_DIR}/src")
set_target_properties(test_about_dialog PROPERTIES AUTOMOC ON)
add_test(NAME test_about_dialog COMMAND test_about_dialog)
set_tests_properties(test_about_dialog PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Layer 3: Integration tests.
add_subdirectory(integration)

# Layer 4: Smoke tests (E2E launch of the real Margin binary).
add_subdirectory(smoke)
