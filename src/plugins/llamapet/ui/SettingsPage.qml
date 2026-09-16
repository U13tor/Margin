import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Item {
    id: root
    width: parent ? parent.width : 640
    height: parent ? parent.height : 720

    property string currentEngine: (typeof telemetryService !== "undefined" && telemetryService && telemetryService.config)
                                   ? telemetryService.config.inference.engine : "llama.cpp"

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: Theme.space4
        contentWidth: width
        contentHeight: settingsCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: settingsCol
            width: flick.width - (scrollIndicator.visible ? 10 : 0)
            spacing: Theme.space4

            // Page Title
            ColumnLayout {
                spacing: 4
                Text {
                    text: qsTr("LlamaPet Settings")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textLg
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Configure local LLM inference backend, GPU telemetry, and desktop pet behaviors.")
                    color: Theme.fgMuted
                    font.pixelSize: Theme.textSm
                }
            }

            // ── Section 1: Inference Engine Card ─────────────────────────────
            MCard {
                id: engineCard
                Layout.fillWidth: true
                implicitHeight: engineCol.implicitHeight + 2 * padding

                ColumnLayout {
                    id: engineCol
                    anchors.fill: parent
                    spacing: Theme.space3

                    Text {
                        text: qsTr("Inference Engine")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textSm
                        font.weight: Font.DemiBold
                    }

                    // Engine Selector Pills
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: [
                                { key: "llama.cpp", label: "llama.cpp" },
                                { key: "ollama", label: "Ollama" },
                                { key: "openai_compatible", label: qsTr("OpenAI Compatible") }
                            ]
                            delegate: MButton {
                                text: modelData.label
                                variant: root.currentEngine === modelData.key ? MButton.Variant.Primary : MButton.Variant.Secondary
                                onClicked: {
                                    root.currentEngine = modelData.key;
                                    if (typeof settings !== "undefined" && settings) {
                                        settings.set("plugins.llamapet.engine", modelData.key);
                                    }
                                }
                            }
                        }
                    }

                    // Endpoint URL Field
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("Endpoint URL")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.textXs
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: qsTr("Loopback only (127.0.0.1 / localhost)")
                                color: Theme.fgMuted
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 34
                            radius: Theme.radiusSm
                            color: Theme.bgBase
                            border.color: urlInput.activeFocus ? Theme.accentBrand : Theme.borderSubtle

                            TextInput {
                                id: urlInput
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: TextInput.AlignVCenter
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                                font.family: Theme.fontMono
                                text: (typeof settings !== "undefined" && settings) ? settings.get("plugins.llamapet.endpointUrl", "http://127.0.0.1:1802") : "http://127.0.0.1:1802"
                                onEditingFinished: {
                                    if (typeof settings !== "undefined" && settings) {
                                        settings.set("plugins.llamapet.endpointUrl", text.trim());
                                    }
                                }
                            }
                        }
                    }

                    // API Key Field
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("API Key")
                                color: Theme.fgSecondary
                                font.pixelSize: Theme.textXs
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: qsTr("Encrypted via OS Keyring")
                                color: Theme.accentSuccess
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 34
                            radius: Theme.radiusSm
                            color: Theme.bgBase
                            border.color: keyInput.activeFocus ? Theme.accentBrand : Theme.borderSubtle

                            TextInput {
                                id: keyInput
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: TextInput.AlignVCenter
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                                echoMode: TextInput.Password
                                text: (typeof settings !== "undefined" && settings) ? settings.get("plugins.llamapet.apiKey", "") : ""
                                onEditingFinished: {
                                    if (typeof settings !== "undefined" && settings) {
                                        settings.set("plugins.llamapet.apiKey", text.trim());
                                    }
                                }
                            }
                        }
                    }

                    // Poll Interval Slider
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("Poll Interval")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.textXs
                            Layout.preferredWidth: 80
                        }

                        MSlider {
                            id: intervalSlider
                            Layout.fillWidth: true
                            from: 200
                            to: 3000
                            stepSize: 100
                            value: (typeof settings !== "undefined" && settings) ? settings.get("plugins.llamapet.pollIntervalMs", 1000) : 1000
                            onMoved: function(v) {
                                if (typeof settings !== "undefined" && settings) {
                                    settings.set("plugins.llamapet.pollIntervalMs", v);
                                }
                            }
                        }

                        Text {
                            text: qsTr("%1 ms").arg(intervalSlider.value)
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.textXs
                            font.family: Theme.fontMono
                            Layout.preferredWidth: 60
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // ── Section 2: Telemetry & Alert Thresholds Card ─────────────────
            MCard {
                id: thresholdCard
                Layout.fillWidth: true
                implicitHeight: thresholdCol.implicitHeight + 2 * padding

                ColumnLayout {
                    id: thresholdCol
                    anchors.fill: parent
                    spacing: Theme.space3

                    Text {
                        text: qsTr("VRAM Alert Thresholds")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textSm
                        font.weight: Font.DemiBold
                    }

                    // Warning Threshold
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("Warning")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.textXs
                            Layout.preferredWidth: 80
                        }

                        MSlider {
                            id: warnSlider
                            Layout.fillWidth: true
                            from: 50
                            to: 95
                            stepSize: 1
                            value: {
                                var v = (typeof settings !== "undefined" && settings) ? settings.get("plugins.llamapet.vramWarn", 0.90) : 0.90;
                                return v <= 1.0 ? Math.round(v * 100) : v;
                            }
                            onMoved: function(v) {
                                if (typeof settings !== "undefined" && settings) {
                                    settings.set("plugins.llamapet.vramWarn", v / 100.0);
                                }
                            }
                        }

                        Text {
                            text: qsTr("%1%").arg(warnSlider.value)
                            color: "#FFAA00"
                            font.pixelSize: Theme.textXs
                            font.family: Theme.fontMono
                            font.weight: Font.DemiBold
                            Layout.preferredWidth: 40
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Critical Threshold
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("Critical")
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.textXs
                            Layout.preferredWidth: 80
                        }

                        MSlider {
                            id: dangerSlider
                            Layout.fillWidth: true
                            from: 60
                            to: 100
                            stepSize: 1
                            value: {
                                var v = (typeof settings !== "undefined" && settings) ? settings.get("plugins.llamapet.vramDanger", 0.96) : 0.96;
                                return v <= 1.0 ? Math.round(v * 100) : v;
                            }
                            onMoved: function(v) {
                                if (typeof settings !== "undefined" && settings) {
                                    settings.set("plugins.llamapet.vramDanger", v / 100.0);
                                }
                            }
                        }

                        Text {
                            text: qsTr("%1%").arg(dangerSlider.value)
                            color: "#FF3366"
                            font.pixelSize: Theme.textXs
                            font.family: Theme.fontMono
                            font.weight: Font.DemiBold
                            Layout.preferredWidth: 40
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // ── Section 3: Desktop Pet & Window Behavior Card ────────────────
            MCard {
                id: windowCard
                Layout.fillWidth: true
                implicitHeight: windowCol.implicitHeight + 2 * padding

                ColumnLayout {
                    id: windowCol
                    anchors.fill: parent
                    spacing: Theme.space3

                    Text {
                        text: qsTr("Desktop Pet Window")
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textSm
                        font.weight: Font.DemiBold
                    }

                    // Always on Top
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Always on Top")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                            }
                            Text {
                                text: qsTr("Keep the floating pet window above all other windows")
                                color: Theme.fgMuted
                                font.pixelSize: Theme.textXs
                            }
                        }
                        MSwitch {
                            checked: true
                            onToggled: function(val) {
                                if (typeof llamapet !== "undefined" && llamapet) {
                                    llamapet.setAlwaysOnTop(val);
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                    // Click-through
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Click-through")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                            }
                            Text {
                                text: qsTr("Pass mouse events through the window to applications beneath")
                                color: Theme.fgMuted
                                font.pixelSize: Theme.textXs
                            }
                        }
                        MSwitch {
                            checked: false
                            onToggled: function(val) {
                                if (typeof llamapet !== "undefined" && llamapet) {
                                    llamapet.setClickThrough(val);
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                    // Auto-hide on Edge (Peek)
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Auto-hide on Edge (Peek)")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                            }
                            Text {
                                text: qsTr("Slide pet into the screen edge after 3s of mouse inactivity")
                                color: Theme.fgMuted
                                font.pixelSize: Theme.textXs
                            }
                        }
                        MSwitch {
                            checked: true
                            onToggled: function(val) {
                                if (typeof llamapet !== "undefined" && llamapet) {
                                    llamapet.setAutoDockHide(val);
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                    // Global Shortcut Row
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Global Shortcut")
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textSm
                            }
                            Text {
                                text: qsTr("Toggle pet window visibility from anywhere")
                                color: Theme.fgMuted
                                font.pixelSize: Theme.textXs
                            }
                        }

                        Rectangle {
                            height: 24
                            radius: 4
                            width: hotkeyText.implicitWidth + 16
                            color: Theme.bgBase
                            border.color: Theme.borderStrong

                            Text {
                                id: hotkeyText
                                anchors.centerIn: parent
                                text: "Ctrl+Alt+P"
                                color: Theme.fgPrimary
                                font.pixelSize: Theme.textXs
                                font.family: Theme.fontMono
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }
        }
    }

    // Modern floating dark scroll indicator
    Rectangle {
        id: scrollIndicator
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.top: parent.top
        anchors.topMargin: Theme.space4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space4
        width: 4
        color: "transparent"
        visible: flick.visibleArea.heightRatio < 1.0

        Rectangle {
            x: 0
            y: flick.visibleArea.yPosition * parent.height
            width: 4
            height: Math.max(flick.visibleArea.heightRatio * parent.height, 24)
            radius: 2
            color: Theme.fgMuted
            opacity: flick.moving || flick.dragging ? 0.6 : 0.25
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }
    }
}
