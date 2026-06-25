# cmake/plugin_i18n.cmake
#
# PR2: plugin translation pipeline. Per-plugin equivalent of build_i18n.cmake.
# Each plugin's CMakeLists.txt calls margin_plugin_i18n(<target> <plugin_id>)
# to:
#   1. Declare lrelease custom commands for i18n/<plugin_id>_{en,zh_CN}.ts
#   2. file(WRITE) a per-plugin qrc at
#      ${BUILD}/plugins/<id>/<id>_i18n.qrc with prefix="/<plugin_id>/i18n"
#      and aliases <plugin_id>_<lang>.qm
#   3. Add the qrc to the plugin target's sources (AUTORCC embeds it)
#   4. Make the target depend on the per-plugin qm custom target
#
# Resource layout (matches QTranslator::load path used by PluginManager):
#   :/<plugin_id>/i18n/<plugin_id>_en.qm
#   :/<plugin_id>/i18n/<plugin_id>_zh_CN.qm
#
# PluginManager loads each catalog via QTranslator at plugin load + on
# general.language changes (setLanguage). Plugins do NOT manage their own
# translator lifetime — the host orchestrates load/remove per plugin.
#
# Adding a new plugin string:
#   - QML: wrap with qsTr("...") in qml/*.qml
#   - C++: QCoreApplication::translate("<PluginName>Plugin", "...")
#   - Run scripts/regen-ts.sh (extended in PR2 to scan plugin qrcs) to
#     append <message type="unfinished"> entries to the plugin's .ts
#   - Edit .ts to fill translations, rebuild — lrelease + AUTORCC handle
#     the rest.

function(margin_plugin_i18n target plugin_id)
    set(_ts_dir "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
    set(_qm_dir "${CMAKE_BINARY_DIR}/plugins/${plugin_id}/i18n")
    file(MAKE_DIRECTORY "${_qm_dir}")

    set(_ts_files
        "${_ts_dir}/${plugin_id}_en.ts"
        "${_ts_dir}/${plugin_id}_zh_CN.ts")
    set(_qm_files "")
    set(_qrc_entries "")

    # Qt6::lrelease comes from the top-level find_package(Qt6 LinguistTools).
    # Custom commands depend on the .ts so CMake/ninja re-run lrelease only
    # when sources change; the qrc lists .qm outputs by absolute path so
    # AUTORCC picks them up. Same pattern as cmake/build_i18n.cmake.
    foreach(_ts ${_ts_files})
        get_filename_component(_base "${_ts}" NAME_WE)  # <plugin_id>_<lang>
        set(_qm "${_qm_dir}/${_base}.qm")
        list(APPEND _qm_files "${_qm}")

        add_custom_command(
            OUTPUT "${_qm}"
            COMMAND Qt6::lrelease "${_ts}" -qm "${_qm}"
            DEPENDS "${_ts}"
            COMMENT "lrelease: ${_base}.qm"
            VERBATIM)

        string(APPEND _qrc_entries
            "        <file alias=\"${_base}.qm\">${_qm}</file>\n")
    endforeach()

    set(_qrc_path "${CMAKE_BINARY_DIR}/plugins/${plugin_id}/${plugin_id}_i18n.qrc")
    file(WRITE "${_qrc_path}"
"<!DOCTYPE RCC>
<RCC version=\"1.0\">
    <qresource prefix=\"/${plugin_id}/i18n\">
${_qrc_entries}    </qresource>
</RCC>
")

    # Adding the qrc to target_sources + AUTORCC (top-level CMAKE_AUTORCC ON)
    # embeds the .qm into the plugin DLL. add_dependencies guarantees the
    # .qm land before AUTORCC tries to compile the qrc (docs/15-A6 risk).
    target_sources(${target} PRIVATE "${_qrc_path}")
    add_custom_target(${target}_i18n_qm ALL DEPENDS ${_qm_files})
    add_dependencies(${target} ${target}_i18n_qm)
endfunction()
