// ExportClearDialog — M2-C7 export / clear-all UI.
//
// Triggered from ScreenTimeTab "..." menu. Three actions:
//   - Export JSON: FileDialog → screen_time.exportJson(url) → success/error toast
//   - Export CSV:  FileDialog → screen_time.exportCsv(url) → success/error toast
//   - Clear all:   confirmation Dialog → screen_time.clearAllData() → closes
//
// window_title is decrypted on export — docs/07 §M2 documents this as
// the explicit "data belongs to the user, export path is user-chosen"
// privacy tradeoff.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Margin.Ui.Primitives

Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 100

    // Public API — ScreenTimeTab toggles `visible` to show the sheet.
    signal closed()

    function open() { root.visible = true; refreshCount(); }

    function refreshCount() {
        sessionCountText.text = qsTr("当前已记录 %1 条 session").arg(screen_time.sessionCount())
    }

    // Dimmed overlay; clicking outside cancels.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        MouseArea {
            anchors.fill: parent
            onClicked: { root.visible = false; root.closed() }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 420
        height: column.implicitHeight + 32
        radius: 8
        color: Theme.bgElevated
        border.color: Theme.borderSubtle
        border.width: 1

        ColumnLayout {
            id: column
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.margins: 16
            width: parent.width - 32
            spacing: 12

            Text {
                text: qsTr("数据导出 / 清除")
                color: Theme.fgPrimary
                font.pixelSize: Theme.textBase
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("导出会把每条 session 的窗口标题解密成明文 — 数据属于你,但请注意保存路径的安全。")
                color: Theme.fgSecondary
                font.pixelSize: Theme.textXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                id: sessionCountText
                color: Theme.fgMuted
                font.pixelSize: Theme.textXs
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MButton {
                    objectName: "exportJsonButton"
                    variant: MButton.Variant.Secondary
                    iconSource: "qrc:/icons/icon-export.svg"
                    text: qsTr("导出 JSON")
                    Layout.fillWidth: true
                    onClicked: jsonSaver.open()
                }
                MButton {
                    objectName: "exportCsvButton"
                    variant: MButton.Variant.Secondary
                    iconSource: "qrc:/icons/icon-export.svg"
                    text: qsTr("导出 CSV")
                    Layout.fillWidth: true
                    onClicked: csvSaver.open()
                }
            }

            // Destructive action: Ghost + trash icon is the universal danger
            // affordance; the confirmDialog below is the real safety net.
            // MButton has no Danger variant by design (docs/12 deferred).
            MButton {
                objectName: "clearAllButton"
                variant: MButton.Variant.Ghost
                iconSource: "qrc:/icons/icon-trash.svg"
                text: qsTr("清除全部数据")
                Layout.fillWidth: true
                onClicked: confirmDialog.open()
            }

            MButton {
                objectName: "closeButton"
                variant: MButton.Variant.Ghost
                iconSource: "qrc:/icons/icon-close.svg"
                text: qsTr("关闭")
                Layout.fillWidth: true
                onClicked: { root.visible = false; root.closed() }
            }
        }
    }

    FileDialog {
        id: jsonSaver
        title: qsTr("导出 JSON")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [ qsTr("JSON 文件 (*.json)"), qsTr("所有文件 (*)") ]
        onAccepted: {
            const err = screen_time.exportJson(currentFile)
            err.length > 0 ? errorToast(err) : okToast(qsTr("JSON 已导出"))
            root.refreshCount()
        }
    }

    FileDialog {
        id: csvSaver
        title: qsTr("导出 CSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [ qsTr("CSV 文件 (*.csv)"), qsTr("所有文件 (*)") ]
        onAccepted: {
            const err = screen_time.exportCsv(currentFile)
            err.length > 0 ? errorToast(err) : okToast(qsTr("CSV 已导出"))
            root.refreshCount()
        }
    }

    Dialog {
        id: confirmDialog
        objectName: "confirmDialog"
        anchors.centerIn: parent
        width: 360
        modal: true
        title: qsTr("确认清除?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            if (screen_time.clearAllData()) {
                okToast(qsTr("已清除"))
            } else {
                errorToast(qsTr("清除失败 — 见日志"))
            }
            root.refreshCount()
        }

        contentItem: Text {
            text: qsTr("将永久删除全部 session 与 pickup 记录,无法恢复。")
            color: Theme.fgPrimary
            wrapMode: Text.WordWrap
        }
    }

    // Tiny inline toast — pops a green or red line for ~2s.
    function okToast(msg)   { toast.text = msg; toast.ok = true;  toast.show(); }
    function errorToast(msg){ toast.text = msg; toast.ok = false; toast.show(); }

    Rectangle {
        id: toast
        property string text: ""
        property bool ok: true
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24
        width: Math.min(360, toastLabel.implicitWidth + 24)
        height: 36
        radius: 4
        color: ok ? Theme.accentSuccess : Theme.accentDanger
        opacity: 0
        visible: opacity > 0

        function show() {
            toast.opacity = 1
            toastHide.start()
        }
        Timer {
            id: toastHide
            interval: 2000
            repeat: false
            onTriggered: toast.opacity = 0
        }
        Behavior on opacity { NumberAnimation { duration: 150 } }

        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: toast.text
            color: Theme.bgBase
            font.pixelSize: Theme.textXs
        }
    }
}
