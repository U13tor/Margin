// AuraTab — Tab4 root (M1-C6 + M4-C14a fidelity pass). StackView swaps:
//   - main view (status dot card + event log + "Settings" button)
//   - settings page (scan + device picker + threshold sliders)
//
// `aura` is the context property registered by AuraLockerPlugin::onLoad
// via QmlService::registerContextProperty. All property reads / invocations
// route through that object.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Margin.Ui.Primitives

Rectangle {
    objectName: "auraTab"
    color: Theme.bgBase

    width: parent ? parent.width : 480
    height: parent ? parent.height : 720

    // Time-formatter helper — Qt.formatDateTime accepts a Date + template.
    // Returns "HH:MM:SS" for ms-precision wall-clock reads in the event log.
    function formatTime(ms) {
        if (ms <= 0) return ""
        return Qt.formatDateTime(new Date(ms), "HH:mm:ss")
    }

    // Leading-icon per event kind — maps an event trail entry to a qrc:/icons
    // URL. Returns "" for kinds without a fitting icon so MListItem hides the
    // icon slot instead of rendering an empty Image.
    function iconForKind(kind) {
        switch (kind) {
            case "lock":    return "qrc:/icons/icon-lock.svg"
            case "away":    return "qrc:/icons/icon-arrow-right.svg"
            case "back":    return "qrc:/icons/icon-check.svg"
            case "warning": return "qrc:/icons/icon-pulse.svg"
            case "paired":  return "qrc:/icons/icon-check.svg"
            default:        return ""
        }
    }

    // State-dot color mapping (●). Keyed on the four proximity states
    // AuraLockerPlugin emits (paired / away / cooldown / inactive). Green =
    // actively tracking + device in range; warning = away or post-lock
    // cooldown; muted = fresh load, no samples yet, or BT radio off (all
    // read as "system not actively tracking" to the user).
    function stateColor(state) {
        switch (state) {
            case "paired":  return Theme.accentSuccess
            case "away":    return Theme.accentWarning
            case "cooldown":return Theme.accentWarning
            default:        return Theme.fgMuted  // inactive / unknown
        }
    }

    StackView {
        id: stack
        objectName: "stack"
        anchors.fill: parent
        initialItem: mainView
    }

    Component {
        id: mainView

        // Item wrapper — StackView's initialItem is managed by the StackView
        // itself; giving the child anchors.fill triggers the "conflicting
        // anchors" warning. The inner ColumnLayout fills this Item instead.
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: qsTr("Aura Locker")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textBase
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Bluetooth Proximity Lock")
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textXs
                }

                MCard {
                    objectName: "auraStatusCard"
                    Layout.fillWidth: true
                    implicitHeight: statusLayout.implicitHeight + 2 * padding

                    ColumnLayout {
                        id: statusLayout
                        anchors.fill: parent
                        spacing: Theme.space2

                        // Status dot + state label — the at-a-glance answer
                        // to "is it watching?". Dot color flips with the four
                        // proximityState values (see stateColor()).
                        RowLayout {
                            spacing: Theme.space2

                            Rectangle {
                                objectName: "stateDot"
                                width: 10
                                height: 10
                                radius: 5
                                color: stateColor(aura.proximityState)
                            }

                            Text {
                                objectName: "stateLabel"
                                text: qsTr("State: %1").arg(aura.proximityState)
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                                font.weight: Font.DemiBold
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            text: aura.pairedDeviceName.length > 0
                                  ? qsTr("Paired: %1").arg(aura.pairedDeviceName)
                                  : qsTr("Not paired")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.textSm
                        }

                        // Single "did it ever lock?" line — the most common
                        // dogfood question. Empty (hidden) until the first
                        // successful lock fires; once present, stays visible
                        // as a one-line at-a-glance status.
                        Text {
                            visible: aura.lastLockMs > 0
                            text: qsTr("Last lock: %1").arg(formatTime(aura.lastLockMs))
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.textSm
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: aura.pairedDeviceName.length > 0
                                  ? qsTr("Margin will lock the screen when this device moves away for %1 seconds.").arg(aura.awayDelaySec)
                                  : qsTr("Open Settings to scan and pair a Bluetooth device.")
                            color: Theme.fgMuted
                            font.pixelSize: Theme.textXs
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // Event trail — newest at top, scrollable if it overflows.
                // Gives the user a visible "did it fire?" trail without
                // forcing them to tail the log file.
                Text {
                    text: qsTr("Recent activity")
                    color: Theme.fgSecondary
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                    visible: aura.recentEvents.length > 0
                }

                ListView {
                    objectName: "eventList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    visible: aura.recentEvents.length > 0
                    model: aura.recentEvents
                    spacing: Theme.space1

                    delegate: MListItem {
                        objectName: "eventItem_" + index
                        width: ListView.view.width
                        title: modelData.message
                        subtitle: formatTime(modelData.timestampMs)
                        iconSource: iconForKind(modelData.kind)
                    }
                }

                MButton {
                    objectName: "openSettingsButton"
                    variant: MButton.Variant.Primary
                    text: qsTr("Settings")
                    iconSource: "qrc:/icons/icon-arrow-right.svg"
                    // M5-C4a: opens the top-level Settings window. PR2: pass
                    // pageId "aura" (mirrors SettingsPageContributor::PageInfo.id
                    // in AuraLockerPlugin.cpp:676) so the sidebar pre-selects
                    // the Aura Locker entry instead of defaulting to General.
                    // typeof guard matches DashboardWindow.qml's Ctrl+, shortcut.
                    onClicked: {
                        if (typeof settingsRoot !== "undefined" && settingsRoot) {
                            settingsRoot.openSettings("aura")
                        }
                    }
                }
            }
        }
    }
}
