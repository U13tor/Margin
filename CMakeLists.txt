cmake_minimum_required(VERSION 3.21)

project(Margin
    VERSION 0.1.0
    DESCRIPTION "Margin / 息间 — a desktop rhythm conductor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Per-build stamp baked into Margin.exe VERSIONINFO FileVersion (right-click →
# Properties → Details) so each build is self-differentiating without manual
# version bumps. Local time (dev-machine perspective). Re-evaluated at every
# configure — any CMakeLists / .cmake / new source edit triggers CMake auto
# re-configure on next build, so the stamp tracks code changes in practice.
# A no-op rebuild with zero source edits keeps the old stamp, which is
# correct (identical input → identical output).
#
# Format: YYYYMMDDHHMMSS (14 digits, second-precision, no separators) so the
# FileVersion reads cleanly as `<release>+build.20260629100526` and sorts
# lexically. Used only in FileVersion strings (version.rc.in + install.nsi.in);
# OutFile / ProductVersion use bare release version without stamp.
string(TIMESTAMP MARGIN_BUILD_STAMP "%Y%m%d%H%M%S")

# Release version override: CI passes -DMARGIN_RELEASE_VERSION=<git-tag>
# (e.g. 0.1.1-rc1) so NSIS OutFile, install.nsi APP_VERSION, and Margin.exe
# VERSIONINFO all reflect the actually-published tag. Defaults to
# PROJECT_VERSION for local dev builds (keeping 0.1.0 baseline).
# project(VERSION ...) rejects SemVer pre-release suffixes like "-rc1", so
# we keep project() numeric and carry the full release string in a separate
# variable that flows into all release-artifact version surfaces.
if(NOT DEFINED MARGIN_RELEASE_VERSION)
    set(MARGIN_RELEASE_VERSION "${PROJECT_VERSION}")
endif()
# Numeric X.Y.Z parsed from MARGIN_RELEASE_VERSION, with any pre-release
# suffix stripped. Required by Windows VIProductVersion / FILEVERSION which
# mandate X.Y.Z.W numeric form (NSIS rejects "0.1.1-rc1.0" at makensis time).
# Falls back to 0.0.0 when MARGIN_RELEASE_VERSION isn't SemVer-shaped (e.g.
# workflow_dispatch dry-run with inputs.version="dry-run") so the numeric
# fields stay valid; the release-string fields (APP_VERSION, ProductVersion)
# still carry the original value verbatim.
if(MARGIN_RELEASE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(MARGIN_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(MARGIN_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(MARGIN_VERSION_PATCH "${CMAKE_MATCH_3}")
else()
    set(MARGIN_VERSION_MAJOR 0)
    set(MARGIN_VERSION_MINOR 0)
    set(MARGIN_VERSION_PATCH 0)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(MARGIN_BUILD_TESTS "Build Margin tests" ON)

# M1-C1+C2: C++/WinRT projection headers ship with the Windows SDK
# itself (no vcpkg dep). Discover the latest installed SDK's cppwinrt
# dir once and expose it via an INTERFACE library so the aura_locker
# plugin and its unit test can both link it without re-running the
# glob. windowsapp (WindowsApp.lib) provides RoActivateInstance and
# the other WinRT runtime stubs.
if(WIN32)
    file(GLOB _cppwinrt_dirs "C:/Program Files (x86)/Windows Kits/10/Include/*/cppwinrt")
    list(SORT _cppwinrt_dirs COMPARE STRING ORDER DESCENDING)
    list(GET _cppwinrt_dirs 0 _margin_cppwinrt_inc)
    add_library(margin_cppwinrt_iface INTERFACE)
    target_include_directories(margin_cppwinrt_iface INTERFACE "${_margin_cppwinrt_inc}")
    target_link_libraries(margin_cppwinrt_iface INTERFACE windowsapp)
endif()

# M4-C2: fetch Inter + JetBrains Mono TTFs into src/resources/fonts/ before any
# target references fonts.qrc. Idempotent — skips on subsequent configures.
# Generates ${CMAKE_BINARY_DIR}/fonts.qrc with entries for every file that
# landed; exposes the path via MARGIN_FONTS_QRC.
include(${CMAKE_SOURCE_DIR}/cmake/fetch_fonts.cmake)

# PR1 i18n fix: LinguistTools provides the Qt6::lrelease imported target used
# by cmake/build_i18n.cmake. We deliberately do NOT use qt6_add_translations
# here — on Qt 6.5.3 it generates an orphan <target>_lrelease utility target
# and never actually embeds .qm into the binary (see docs/15-A8). Instead we
# use the fetch_fonts.cmake pattern: configure-time file(WRITE) of i18n.qrc
# + per-.ts custom commands + AUTORCC. Language switcher (Settings → General
# → Language) reloads :/i18n/host_<lang>.qm via QTranslator + retranlates
# the QML engine without restart (HostCore::applyLanguage).
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
include(${CMAKE_SOURCE_DIR}/cmake/build_i18n.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/plugin_i18n.cmake)  # PR2: margin_plugin_i18n() for plugins

# Python interpreter — used by cmake/gen_nsis_license.py (M6 NSIS UTF-16
# license copy). Smoke tests already require Python on the dev machine,
# so this is not a new dependency. Python3::Interpreter from FindPython3
# gives us Python_EXECUTABLE on all platforms.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# M0-C6: plugin_api (STATIC for EventBus.cpp) + paths + host (with HostServicesImpl
# + PluginManager) + tests skeleton (unit + integration) + plugins/hello (reference
# TrayMenuContributor consumer).
# M0-C10a: src/ui (Margin.Ui.Primitives + Margin.Ui.Composite qml modules; shell
# QML compiled into the Margin exe via ui.qrc). Added before src/host so the
# module targets exist when the executable links their plugins.
add_subdirectory(src/plugin_api)
add_subdirectory(src/paths)
add_subdirectory(src/ui)
add_subdirectory(src/host)
add_subdirectory(src/plugins/hello)
add_subdirectory(src/plugins/aura_locker)  # M1-C5 skeleton (BLE wiring in M1-C1/C2)
add_subdirectory(src/plugins/screen_time)  # M2-C8 skeleton (DB wire-up in M2-C3)
add_subdirectory(src/plugins/rhythm)       # M3-C1 skeleton (overlay + Lottie wiring in M3-C4)

if(MARGIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)  # M0-C4 (was M0-C9 — moved up for EventBus unit tests)
endif()

# M6-C3: Windows install rules + CPack NSIS configuration.
# Per-user install to %LOCALAPPDATA%\Margin (no UAC) — see docs/02-source-layout.md §2.1.
# macOS packaging is out of scope for v1.0 (docs/12-deferred-items.md; user
# explicitly scoped release to Windows-only).
if(WIN32)
    # 1) Margin.exe only. Qt runtime is deployed at install time via
    #    windeployqt (step 4 below) so the build dir stays clean of clutter
    #    in the install manifest — only files we explicitly install end up
    #    in the NSIS package.
    install(TARGETS Margin RUNTIME DESTINATION .)

    # 2) Licenses — LGPL (host) + font OFL. OpenSSL was removed in D5-1
    #    (Qt HMAC + Win CNG replace it), so its DLLs / NOTICE / Apache-2.0
    #    full text no longer ship. See docs/02-source-layout.md §2.1.
    install(FILES LICENSE DESTINATION licenses)
    install(DIRECTORY src/resources/fonts/ DESTINATION licenses
            FILES_MATCHING PATTERN "LICENSE-*.txt")

    # 3) Official plugins. Built to <build>/src/host/plugins/ by each
    #    plugin's POST_BUILD step (see src/plugins/*/CMakeLists.txt) and
    #    scanned at runtime by PluginManager via Paths::officialPlugins()
    #    (= applicationDirPath() + "/plugins"). The hello reference
    #    plugin is intentionally EXCLUDE'd — it's a dev-only sample
    #    (docs/04-plugin-spec.md §8) and must not ship in the installer.
    #    The EXCLUDE is defensive: hello's POST_BUILD can populate the
    #    staging dir under some build orderings; making the exclusion
    #    explicit here means the rule stays correct regardless.
    install(DIRECTORY ${CMAKE_BINARY_DIR}/src/host/plugins/
            DESTINATION plugins
            FILES_MATCHING
                PATTERN "*.dll"
                PATTERN "*.manifest.json"
            REGEX "margin-hello" EXCLUDE)

    # 4) windeployqt at install time. Runs against the staged Margin.exe in
    #    ${CMAKE_INSTALL_PREFIX}, so the Qt DLLs / platforms / tls / etc.
    #    land inside the install tree that NSIS then packages into the
    #    installer.
    #
    #    --qmldir for each QML source dir is required because Margin embeds
    #    QML via qrc (qrc:/ui/DashboardWindow.qml, qrc:/rhythm/qml/*.qml),
    #    and windeployqt's binary scan can't see imports that originate
    #    from qrc-embedded sources — only from .qml files on disk. Without
    #    it, qml/QtQuick/Layouts/, qml/QtQuick/Window/ etc. are skipped
    #    and the runtime errors with "module QtQuick.Layouts is not
    #    installed" (observed post-install, 2026-06-24).
    install(CODE "
        set(_qml_dirs
            \"${CMAKE_SOURCE_DIR}/src/ui\"
            \"${CMAKE_SOURCE_DIR}/src/ui/primitives\"
            \"${CMAKE_SOURCE_DIR}/src/ui/composite\"
            \"${CMAKE_SOURCE_DIR}/src/ui/charts\"
            \"${CMAKE_SOURCE_DIR}/src/plugins/rhythm/qml\"
            \"${CMAKE_SOURCE_DIR}/src/plugins/aura_locker/qml\"
            \"${CMAKE_SOURCE_DIR}/src/plugins/screen_time/qml\")
        set(_qmldir_args \"\")
        foreach(_d \${_qml_dirs})
            list(APPEND _qmldir_args \"--qmldir\" \"\${_d}\")
        endforeach()
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    \${_qmldir_args}
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --release
                    \"\${CMAKE_INSTALL_PREFIX}/Margin.exe\"
            RESULT_VARIABLE _wdq_rc)
        if(NOT _wdq_rc EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed (exit \${_wdq_rc})\")
        endif()
    ")

    # 5) NSIS installer. CMake's stock NSIS.template.in hardcodes
    #    `RequestExecutionLevel admin` (triggers UAC) AND runs a
    #    SetShellVarContext all dance in .onInit that flips $LOCALAPPDATA
    #    to C:\ProgramData on admin accounts. `CPACK_NSIS_TEMPLATE_FILE`
    #    is documented to override the template but does not take effect
    #    on CMake 4.0's NSIS generator (verified across 3 attempt paths,
    #    see docs/posts/2026-06-24-nsis-installer-handoff.md). We bypass
    #    CPack NSIS entirely: drive makensis against our own
    #    packaging/install.nsi.in, which controls RequestExecutionLevel +
    #    InstallDir + license encoding directly.
    #
    #    Flow: configure_file() bakes version + paths into install.nsi;
    #    `cmake --install ... --prefix stage` populates the staging dir;
    #    the `package_nsis` custom target chains both and runs makensis.
    #
    #    License: UTF-8 LICENSE is converted to a UTF-16 LE BOM copy at
    #    configure time (cmake/gen_nsis_license.py). NSIS 3.x auto-detects
    #    the BOM and decodes correctly; without it, "息间" renders as
    #    mojibake (system ANSI codepage = CP936/GBK on Chinese Windows).
    set(_nsis_license "${CMAKE_BINARY_DIR}/license-utf16.txt")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/cmake/gen_nsis_license.py"
                "${CMAKE_SOURCE_DIR}/LICENSE" "${_nsis_license}"
        RESULT_VARIABLE _lic_rc
        OUTPUT_VARIABLE _lic_out
        ERROR_VARIABLE _lic_err)
    if(NOT _lic_rc EQUAL 0)
        message(FATAL_ERROR "gen_nsis_license.py failed (${_lic_rc}): ${_lic_err}")
    endif()

    # Stage dir matches `cmake --install ... --prefix <stage>` invocation
    # used by both local dev and release.yml. We bake its native path into
    # install.nsi so makensis's File /r finds Margin.exe + Qt runtime +
    # plugins/ + licenses/.
    set(MARGIN_NSIS_STAGE_DIR "${CMAKE_BINARY_DIR}/stage")
    set(MARGIN_NSIS_OUT_FILE "${CMAKE_BINARY_DIR}/Margin-Setup-${MARGIN_RELEASE_VERSION}-x64.exe")
    file(TO_NATIVE_PATH "${MARGIN_NSIS_STAGE_DIR}" MARGIN_NSIS_STAGE_DIR_NATIVE)
    file(TO_NATIVE_PATH "${_nsis_license}" MARGIN_NSIS_LICENSE_NATIVE)
    file(TO_NATIVE_PATH "${MARGIN_NSIS_OUT_FILE}" MARGIN_NSIS_OUT_FILE_NATIVE)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/install.nsi.in
                   ${CMAKE_BINARY_DIR}/install.nsi @ONLY)
    # Prepend UTF-8 BOM so makensis detects the script as Unicode instead
    # of falling back to the system ANSI codepage (CP936/GBK on Chinese
    # Windows, which errors out on the em dash / CJK chars in the script
    # — including the user-facing "Margin — 息间" product name).
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "-c"
                "import sys; p=sys.argv[1]; b=open(p,'rb').read(); open(p,'wb').write(b'\\xef\\xbb\\xbf'+b)"
                "${CMAKE_BINARY_DIR}/install.nsi"
        RESULT_VARIABLE _bom_rc)
    if(NOT _bom_rc EQUAL 0)
        message(FATAL_ERROR "Failed to prepend UTF-8 BOM to install.nsi (exit ${_bom_rc})")
    endif()

    # makensis discovery. NSIS 3.x is preinstalled on windows-latest runners
    # and on dev machines that opt into installer packaging; we don't FATAL
    # on missing makensis at configure time so a non-NSIS dev build still
    # succeeds — the error surfaces when `package_nsis` is actually invoked.
    find_program(MAKENSIS_EXECUTABLE NAMES makensis
                 HINTS "C:/Program Files (x86)/NSIS" "C:/Program Files/NSIS")

    add_custom_target(package_nsis
        COMMENT "Staging install tree to ${MARGIN_NSIS_STAGE_DIR}"
        # Stage first — the install(CODE) windeployqt step (step 4 above)
        # runs as part of `cmake --install`, populating Qt DLLs / plugins/ /
        # tls/ next to Margin.exe.
        COMMAND ${CMAKE_COMMAND}
                --install ${CMAKE_BINARY_DIR}
                --prefix "${MARGIN_NSIS_STAGE_DIR}"
                --config $<CONFIG>
        COMMAND ${CMAKE_COMMAND} -E echo "Building NSIS installer..."
        COMMAND "$<IF:$<BOOL:${MAKENSIS_EXECUTABLE}>,${MAKENSIS_EXECUTABLE},${CMAKE_COMMAND}>"
                "$<IF:$<BOOL:${MAKENSIS_EXECUTABLE}>,${CMAKE_BINARY_DIR}/install.nsi,-E echo>"
        # makensis returns nonzero on script error; rely on the build
        # system's status reporting to surface the failure.
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        VERBATIM
        SOURCES ${CMAKE_SOURCE_DIR}/packaging/install.nsi.in)
endif()

message(STATUS "Margin ${MARGIN_RELEASE_VERSION} configured (M0-C10a).")
