// StatusBar — bottom status strip (docs/06 §4.1, height 24, --text-xs).
// Layout: [vX.Y.Z · ● mode · duration]   ...   [GitHub][About][设置]
//
// mode/duration are driven by the rhythm plugin context property (state,
// remainingSeconds, workMinutes, breakMinutes Q_PROPERTYs). typeof guard
// falls back to idle / hidden when rhythm is absent (no plugin loaded,
// or test harness without stub) — same pattern OverviewTab.qml uses.
//
// [GitHub] opens the project URL via Qt.openUrlExternally (hands the
// URL to the OS shell handler — no in-app network). [About] and [设置]
// are visual-only hooks (no-op onClicked) to be wired in the M5
// settings milestone.
import QtQuick
import Margin.Ui.Primitives

Rectangle {
    id: root
    objectName: "statusBar"
    implicitHeight: 24
    color: Theme.bgBase

    property string version: ""

    // ── mode → label + dot color mapping ──
    // idle       → 就绪     / fgMuted
    // working    → 专注模式 / accentBrand
    // break_due  → 休息提醒 / accentWarning
    // break_active → 休息中 / accentSuccess
    readonly property var _modeInfo: {
        const s = (typeof rhythm !== "undefined") ? rhythm.state : "idle"
        switch (s) {
            case "working":      return { label: qsTr("专注模式"), color: Theme.accentBrand }
            case "break_due":    return { label: qsTr("休息提醒"), color: Theme.accentWarning }
            case "break_active": return { label: qsTr("休息中"),   color: Theme.accentSuccess }
            default:             return { label: qsTr("就绪"),     color: Theme.fgMuted }
        }
    }

    // ── duration elapsed in current phase ──
    // working:      workMinutes*60 - remainingSeconds → "Xh Ym" / "Xm"
    // break_active: breakMinutes*60 - remainingSeconds → "Xs" / "Xm Ys" / "Xm"
    // idle / break_due: "" (duration Text hidden)
    readonly property string _duration: {
        if (typeof rhythm === "undefined") return ""
        const s = rhythm.state
        if (s === "working") {
            return _formatHhMm(rhythm.workMinutes * 60 - rhythm.remainingSeconds)
        }
        if (s === "break_active") {
            return _formatMmSs(rhythm.breakMinutes * 60 - rhythm.remainingSeconds)
        }
        return ""
    }

    function _formatHhMm(sec) {
        if (sec < 0) sec = 0
        const h = Math.floor(sec / 3600)
        const m = Math.floor((sec % 3600) / 60)
        return h > 0 ? (h + "h " + m + "m") : (m + "m")
    }

    function _formatMmSs(sec) {
        if (sec < 0) sec = 0
        const m = Math.floor(sec / 60)
        const s = sec % 60
        if (m === 0) return s + "s"
        if (m < 10)  return m + "m " + s + "s"
        return m + "m"
    }



    // ── Left: version + dot + mode + duration ──
    Row {
        id: leftRow
        anchors {
            left: parent.left
            leftMargin: 12
            verticalCenter: parent.verticalCenter
        }
        spacing: 6

        Text {
            objectName: "statusBarVersion"
            text: "v" + root.version
            color: Theme.fgSecondary
            font.pixelSize: Theme.textXs
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: "·"
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 6
            height: 6
            radius: 3
            anchors.verticalCenter: parent.verticalCenter
            color: root._modeInfo.color
        }

        Text {
            text: root._modeInfo.label
            color: Theme.fgSecondary
            font.pixelSize: Theme.textXs
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            objectName: "statusBarDuration"
            text: root._duration
            color: Theme.fgMuted
            font.pixelSize: Theme.textXs
            font.family: Theme.fontMono
            visible: root._duration !== ""
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ── Right: GitHub / About / 设置 link row ──
    Row {
        id: rightRow
        anchors {
            right: parent.right
            rightMargin: 12
            verticalCenter: parent.verticalCenter
        }
        spacing: 12

        ComponentLink {
            objectName: "statusBarLinkGithub"
            iconSource: "qrc:/icons/icon-github.svg"
            label: qsTr("GitHub")
            onClicked: Qt.openUrlExternally("https://github.com/U13tor/Margin")
        }

        // About → Settings → About page (same target as tray About — the
        // legacy native QDialog is retired). typeof guard mirrors
        // DashboardWindow.qml's Ctrl+, shortcut for the test harness case
        // where settingsRoot is not yet registered.
        ComponentLink {
            objectName: "statusBarLinkAbout"
            iconSource: "qrc:/icons/icon-info.svg"
            label: qsTr("About")
            onClicked: {
                if (typeof settingsRoot !== "undefined" && settingsRoot) {
                    settingsRoot.openSettings("about")
                }
            }
        }

        // Settings → top-level Settings window (same as tray Settings and
        // Ctrl+,). Empty pageId keeps sidebar on General.
        ComponentLink {
            objectName: "statusBarLinkSettings"
            iconSource: "qrc:/icons/tab-settings.svg"
            label: qsTr("设置")
            onClicked: {
                if (typeof settingsRoot !== "undefined" && settingsRoot) {
                    settingsRoot.openSettings()
                }
            }
        }
    }

    // ── Reusable inline link atom (icon + text + pointer cursor) ──
    component ComponentLink : Item {
        property string iconSource: ""
        property string label: ""
        signal clicked()

        implicitWidth: linkRow.width
        implicitHeight: 24

        Row {
            id: linkRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            MIcon {
                source: iconSource
                size: 12
                color: Theme.fgSecondary
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: label
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
