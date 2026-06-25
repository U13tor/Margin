# cmake/build_i18n.cmake
#
# PR1: host translation pipeline. Replaces qt6_add_translations(Margin ...)
# which on Qt 6.5.3 generated an orphan Margin_lrelease utility target and
# never actually linked any .qm into the Margin executable (see docs/15-A8).
#
# This module follows the proven fetch_fonts.cmake pattern: at configure
# time we (1) declare per-.ts lrelease custom commands, (2) file(WRITE) a
# generated i18n.qrc listing the .qm outputs, and (3) expose its path via
# MARGIN_I18N_QRC so src/host/CMakeLists.txt can add it to qt_add_executable
# sources. AUTORCC (top-level CMakeLists.txt) embeds the qrc into Margin.exe.
#
# Resource layout:
#   :/i18n/host_en.qm      <- English catalog (host QML + C++ translate calls)
#   :/i18n/host_zh_CN.qm   <- Simplified Chinese catalog
#
# HostCore::applyLanguage loads :/i18n/host_<lang>.qm via QTranslator at
# bootstrap + on general.language changes. QML bindings re-evaluate via
# QQmlApplicationEngine::retranslate.
#
# Plugin translations live separately under :/<plugin_id>/i18n/ (PR2 +
# cmake/plugin_i18n.cmake) — this module only handles the host catalog.
#
# Adding a new host string: wrap with qsTr("...") in a qrc:/ui/*.qml file
# or QCoreApplication::translate("Context", "...") in C++, then run
# scripts/regen-ts.sh — lupdate will append <message type="unfinished">
# entries to i18n/host_*.ts for the translator to fill in.

set(_host_ts_files
    "${CMAKE_SOURCE_DIR}/i18n/host_en.ts"
    "${CMAKE_SOURCE_DIR}/i18n/host_zh_CN.ts")

set(_host_qm_dir "${CMAKE_BINARY_DIR}/i18n")
file(MAKE_DIRECTORY "${_host_qm_dir}")

set(_host_qm_files "")
set(_host_qrc_entries "")

# find_package(Qt6 COMPONENTS LinguistTools) at top-level provides the
# Qt6::lrelease target. Custom commands depend on the .ts so CMake/ninja
# re-run lrelease only when sources change; the qrc lists the .qm outputs
# by absolute path so AUTORCC picks them up.
foreach(_ts ${_host_ts_files})
    get_filename_component(_base "${_ts}" NAME_WE)  # host_en / host_zh_CN
    set(_qm "${_host_qm_dir}/${_base}.qm")
    list(APPEND _host_qm_files "${_qm}")

    add_custom_command(
        OUTPUT "${_qm}"
        COMMAND Qt6::lrelease "${_ts}" -qm "${_qm}"
        DEPENDS "${_ts}"
        COMMENT "lrelease: ${_base}.qm"
        VERBATIM)

    string(APPEND _host_qrc_entries
        "        <file alias=\"${_base}.qm\">${_qm}</file>\n")
endforeach()

# Generated qrc lives in the build tree (mirrors fetch_fonts.cmake) so the
# source tree stays clean. qresource prefix="/i18n" aligns with the
# QTranslator::load(":/i18n/host_<lang>.qm") path in HostCore.cpp.
file(WRITE "${CMAKE_BINARY_DIR}/i18n.qrc"
"<!DOCTYPE RCC>
<RCC version=\"1.0\">
    <qresource prefix=\"/i18n\">
${_host_qrc_entries}    </qresource>
</RCC>
")

# ALL so the qm files exist even on bare `make host_i18n_qm`. Host target
# adds this as a dependency in src/host/CMakeLists.txt to guarantee the
# .qm land before AUTORCC tries to compile i18n.qrc (docs/15-A6 risk).
add_custom_target(host_i18n_qm ALL DEPENDS ${_host_qm_files})

set(MARGIN_I18N_QRC "${CMAKE_BINARY_DIR}/i18n.qrc" CACHE INTERNAL
    "Path to generated i18n.qrc (host translations)")

set(MARGIN_I18N_QM_TARGET host_i18n_qm CACHE INTERNAL
    "Custom target that produces the host .qm files")
