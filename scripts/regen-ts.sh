#!/usr/bin/env bash
# scripts/regen-ts.sh — regenerate host + plugin .ts from current qsTr() calls.
#
# Workflow when you add/modify a qsTr() string in src/ui/*.qml OR in a
# plugin's qml/*.qml:
#   1. Edit the QML
#   2. Run `bash scripts/regen-ts.sh`
#   3. Open the relevant .ts in Qt Linguist (or a text editor), fill the
#      <translation type="unfinished"> entries
#   4. Rebuild — lrelease compiles .ts → .qm and AUTORCC embeds it
#
# This script is idempotent: lupdate preserves existing translations and
# only appends new <message type="unfinished"> entries for novel sources.
# -no-obsolete drops entries whose source no longer appears in the QML.
#
# Scope:
#   - Host QML: src/resources/host.qrc + src/ui/ui.qrc → i18n/host_*.ts
#   - Plugin QML: src/plugins/<id>/*.qrc → src/plugins/<id>/i18n/<id>_*.ts
#     (auto-discovered; add a new plugin drop-in and re-run)
#
# Note: lupdate does not scan QCoreApplication::translate() calls. C++
# contexts (DashboardTabs / SettingsPages / SystemTray / <PluginName>Plugin)
# are hand-maintained at the bottom of the corresponding .ts file.
#
# Qt location: we honor Qt6_DIR if set (build-smoke.sh convention), else
# fall back to the canonical D:/Qt/6.7.3/msvc2019_64 path on Windows. On
# macOS the Qt bin dir is typically already on PATH from Homebrew/Qt Creator.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# Locate lupdate
if [[ -n "${Qt6_DIR:-${QT6_DIR:-}}" ]]; then
    qt_bin="${Qt6_DIR:-${QT6_DIR}}/bin"
else
    qt_bin="${Qt6_DIR:-${QT6_DIR:?set Qt6_DIR env var}}/bin"
fi
lupdate="$qt_bin/lupdate"
if [[ ! -x "$lupdate" && -x "${lupdate}.exe" ]]; then
    lupdate="${lupdate}.exe"
fi
if ! command -v "$lupdate" >/dev/null 2>&1 && [[ ! -x "$lupdate" ]]; then
    echo "error: lupdate not found at $lupdate" >&2
    echo "set Qt6_DIR (e.g. ${Qt6_DIR}) and re-run" >&2
    exit 1
fi

# Host QML surface: ui.qrc covers the shell QML files; host.qrc is icons
# only (no translatable strings).
host_qrc=(
    "src/resources/host.qrc"
    "src/ui/ui.qrc"
)

echo "==> lupdate host zh_CN"
"$lupdate" "${host_qrc[@]}" \
    -ts i18n/host_zh_CN.ts \
    -source-language en -target-language zh_CN \
    -no-obsolete

echo "==> lupdate host en"
"$lupdate" "${host_qrc[@]}" \
    -ts i18n/host_en.ts \
    -source-language en -target-language en \
    -no-obsolete

# Plugin QML surfaces — auto-discover any src/plugins/<id>/i18n/*_en.ts
# and regenerate matching *_zh_CN.ts alongside. The plugin_id (used for the
# .ts filename and the runtime resource prefix :/<id>/i18n/<id>_<lang>.qm)
# may differ from the folder name when the binary is named differently
# from the runtime id (e.g. aura_locker/ folder hosts plugin id "aura").
# Deriving from the existing .ts filename sidesteps the mismatch.
shopt -s nullglob
for plugin_en_ts in src/plugins/*/i18n/*_en.ts; do
    ts_dir="$(dirname "$plugin_en_ts")"
    plugin_id="$(basename "$plugin_en_ts" _en.ts)"
    # Find the plugin's main qrc — by convention <folder>/<folder>.qrc OR
    # <folder>/<plugin_id>.qrc. Fall back to passing the whole folder.
    plugin_dir="$(dirname "$ts_dir")"
    plugin_qrc=""
    for candidate in "$plugin_dir/$(basename "$plugin_dir").qrc" \
                     "$plugin_dir/${plugin_id}.qrc"; do
        if [[ -f "$candidate" ]]; then
            plugin_qrc="$candidate"
            break
        fi
    done
    if [[ -z "$plugin_qrc" ]]; then
        echo "warn: no .qrc found for $plugin_id in $plugin_dir, skipping" >&2
        continue
    fi
    echo "==> lupdate plugin $plugin_id zh_CN"
    "$lupdate" "$plugin_qrc" \
        -ts "$ts_dir/${plugin_id}_zh_CN.ts" \
        -source-language en -target-language zh_CN \
        -no-obsolete
    echo "==> lupdate plugin $plugin_id en"
    "$lupdate" "$plugin_qrc" \
        -ts "$ts_dir/${plugin_id}_en.ts" \
        -source-language en -target-language en \
        -no-obsolete
done

echo
echo "Done. Edit the regenerated .ts files to fill <translation type=\"unfinished\"> entries."
echo "Then run: bash scripts/build-smoke.sh"
