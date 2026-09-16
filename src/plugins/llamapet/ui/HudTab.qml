import QtQuick
import QtQuick.Layouts
import Margin.Ui.Primitives

Rectangle {
    id: root
    color: Theme.bgBase
    clip: true

    width: parent ? parent.width : 480
    height: parent ? parent.height : 720

    // Realtime trend history buffer (up to 60 points)
    property var historyPoints: []
    property int maxHistory: 60

    function formatDialogText() {
        if (!telemetryService) return qsTr("System standby — ready for inference");
        var emotion = telemetryService.emotion;
        var tps = telemetryService.currentTps;
        switch (emotion) {
        case 0: return qsTr("Model sleeping... zzz");
        case 1: return qsTr("Ready for inference!");
        case 2: return qsTr("Processing prompt...");
        case 3: return qsTr("Generating tokens (%1 tok/s)").arg(tps.toFixed(1));
        case 4: return qsTr("✨ KV cache hit!");
        case 5: return qsTr("🚨 VRAM near capacity!");
        case 6: return qsTr("Local service offline");
        default:
            return (telemetryService.dialogText && telemetryService.dialogText.length > 0)
                   ? telemetryService.dialogText : qsTr("System standby — ready for inference");
        }
    }

    Connections {
        target: (typeof telemetryService !== "undefined" && telemetryService) ? telemetryService : null
        function onTelemetryChanged() {
            var vram = telemetryService ? telemetryService.vramPercent : 0;
            var tps = telemetryService ? telemetryService.currentTps : 0;
            var pts = root.historyPoints.slice();
            pts.push({ vram: vram, tps: tps });
            if (pts.length > root.maxHistory) {
                pts.shift();
            }
            root.historyPoints = pts;
            historyCanvas.requestPaint();
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: Theme.space4
        contentWidth: width
        contentHeight: mainCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: mainCol
            width: flick.width - (scrollIndicator.visible ? 10 : 0)
            spacing: Theme.space3

            // 1. Top Header Row
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    text: qsTr("❖ LlamaPet")
                    color: Theme.fgPrimary
                    font.pixelSize: Theme.textBase
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    radius: 4
                    height: 22
                    width: statusText.implicitWidth + 12
                    color: (telemetryService && telemetryService.connected) ? "#1A00FF88" : "#20FF3366"
                    border.color: (telemetryService && telemetryService.connected) ? "#4000FF88" : "#50FF3366"

                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        text: (telemetryService && telemetryService.connected) ? qsTr("Online") : qsTr("Offline")
                        color: (telemetryService && telemetryService.connected) ? "#00FF88" : "#FF3366"
                        font.pixelSize: Theme.textXs
                        font.weight: Font.Medium
                    }
                }
            }

            // 2. GPU Status Card
            MCard {
                id: gpuCard
                Layout.fillWidth: true
                implicitHeight: gpuCol.implicitHeight + 2 * padding

                ColumnLayout {
                    id: gpuCol
                    anchors.fill: parent
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: "#00FF88"
                        }
                        Text {
                            text: (telemetryService && telemetryService.deviceName.length > 0)
                                  ? telemetryService.deviceName : qsTr("NVIDIA GPU initializing...")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.textSm
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("Utilization: %1%").arg(telemetryService ? telemetryService.gpuUtil : 0)
                            color: Theme.fgMuted
                            font.pixelSize: Theme.textXs
                        }
                    }

                    // VRAM Usage Text
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("VRAM: %1 GB / %2 GB (%3%)")
                                  .arg(((telemetryService ? telemetryService.vramUsedMb : 0) / 1024.0).toFixed(2))
                                  .arg(((telemetryService ? telemetryService.vramTotalMb : 0) / 1024.0).toFixed(2))
                                  .arg((telemetryService ? telemetryService.vramPercent : 0).toFixed(1))
                            color: Theme.fgSecondary
                            font.pixelSize: Theme.textXs
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("Temp: %1°C · Power: %2W / %3W")
                                  .arg(telemetryService ? telemetryService.tempC : 0)
                                  .arg((telemetryService ? telemetryService.powerW : 0).toFixed(0))
                                  .arg((telemetryService ? telemetryService.powerLimitW : 0).toFixed(0))
                            color: Theme.fgMuted
                            font.pixelSize: Theme.textXs
                        }
                    }

                    // VRAM Progress Bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 6
                        radius: 3
                        color: "#20FFFFFF"
                        Rectangle {
                            height: parent.height
                            radius: 3
                            width: Math.min(parent.width, Math.max(0, parent.width * ((telemetryService ? telemetryService.vramPercent : 0) / 100.0)))
                            color: {
                                var p = telemetryService ? telemetryService.vramPercent : 0;
                                if (p > 95) return "#FF3366";
                                if (p > 85) return "#FFAA00";
                                return "#00F0FF";
                            }
                        }
                    }
                }
            }

            // 3. Realtime Trend Chart Card (60 samples)
            MCard {
                id: trendCard
                Layout.fillWidth: true
                implicitHeight: trendCol.implicitHeight + 2 * padding

                ColumnLayout {
                    id: trendCol
                    anchors.fill: parent
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Realtime Trend (60 samples)")
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.textXs
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle { width: 8; height: 8; radius: 2; color: "#00F0FF" }
                        Text {
                            text: qsTr("VRAM %1%").arg((telemetryService ? telemetryService.vramPercent : 0).toFixed(1))
                            color: Theme.fgMuted
                            font.pixelSize: Theme.textXs
                        }
                        Rectangle { width: 8; height: 8; radius: 2; color: "#00FF88" }
                        Text {
                            text: qsTr("TPS %1").arg((telemetryService ? telemetryService.currentTps : 0).toFixed(1))
                            color: Theme.fgMuted
                            font.pixelSize: Theme.textXs
                        }
                    }

                    Canvas {
                        id: historyCanvas
                        Layout.fillWidth: true
                        Layout.preferredHeight: 110
                        implicitHeight: 110

                        onPaint: {
                            var ctx = getContext("2d");
                            var w = width;
                            var h = height;
                            ctx.clearRect(0, 0, w, h);

                            // Reference grid lines (50% & 100%)
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.08)";
                            ctx.lineWidth = 1;
                            ctx.setLineDash([2, 2]);

                            ctx.beginPath();
                            ctx.moveTo(0, h * 0.5);
                            ctx.lineTo(w, h * 0.5);
                            ctx.moveTo(0, 2);
                            ctx.lineTo(w, 2);
                            ctx.stroke();
                            ctx.setLineDash([]);

                            var pts = root.historyPoints;
                            if (!pts || pts.length < 2) return;

                            var maxPts = root.maxHistory;
                            var stepX = w / (maxPts - 1);
                            var startOffset = maxPts - pts.length;

                            // 1. Draw VRAM curve (0 - 100%)
                            ctx.beginPath();
                            ctx.strokeStyle = "#00F0FF";
                            ctx.lineWidth = 1.5;
                            for (var i = 0; i < pts.length; ++i) {
                                var x = (startOffset + i) * stepX;
                                var normVram = Math.min(Math.max(pts[i].vram / 100.0, 0), 1);
                                var y = h - normVram * (h - 6) - 3;
                                if (i === 0) ctx.moveTo(x, y);
                                else ctx.lineTo(x, y);
                            }
                            ctx.stroke();

                            // 2. Draw TPS curve (normalized to 50 t/s)
                            ctx.beginPath();
                            ctx.strokeStyle = "#00FF88";
                            ctx.lineWidth = 1.5;
                            for (var j = 0; j < pts.length; ++j) {
                                var tx = (startOffset + j) * stepX;
                                var normTps = Math.min(Math.max(pts[j].tps / 50.0, 0), 1);
                                var ty = h - normTps * (h - 6) - 3;
                                if (j === 0) ctx.moveTo(tx, ty);
                                else ctx.lineTo(tx, ty);
                            }
                            ctx.stroke();
                        }
                    }
                }
            }

            // 4. Slot Matrix Card
            MCard {
                id: slotsCard
                Layout.fillWidth: true
                implicitHeight: slotsCol.implicitHeight + 2 * padding

                ColumnLayout {
                    id: slotsCol
                    anchors.fill: parent
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: (telemetryService && telemetryService.connected) ? "#00FF88" : "#50FFFFFF"
                        }

                        Text {
                            text: telemetryService ? telemetryService.engine : "llama.cpp"
                            color: Theme.fgPrimary
                            font.pixelSize: Theme.textSm
                            font.weight: Font.DemiBold
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: qsTr("%1 tok/s · %2% KV Hit")
                                  .arg((telemetryService ? telemetryService.currentTps : 0).toFixed(1))
                                  .arg((telemetryService ? telemetryService.cacheHitRatePct : 0).toFixed(0))
                            color: "#00FF88"
                            font.pixelSize: Theme.textXs
                            font.family: Theme.fontMono
                            font.weight: Font.Medium
                        }
                    }

                    // Table Header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { Layout.preferredWidth: 46; text: qsTr("SLOT"); color: Theme.fgMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text { Layout.preferredWidth: 68; text: qsTr("STATUS"); color: Theme.fgMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text { Layout.preferredWidth: 88; text: qsTr("CONTEXT"); color: Theme.fgMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text { Layout.preferredWidth: 68; text: qsTr("KV HIT"); color: Theme.fgMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text { Layout.fillWidth: true; text: qsTr("OUTPUT"); horizontalAlignment: Text.AlignRight; color: Theme.fgMuted; font.pixelSize: 10; font.weight: Font.DemiBold }
                    }

                    // Slot Matrix Rows
                    Repeater {
                        model: (telemetryService && telemetryService.slotsList) ? telemetryService.slotsList : []
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            implicitHeight: 28
                            height: 28
                            color: index % 2 === 0 ? "transparent" : "#08FFFFFF"
                            radius: 3

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 4

                                Text {
                                    Layout.preferredWidth: 46
                                    text: "#" + modelData.id
                                    color: Theme.fgPrimary
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    font.family: Theme.fontMono
                                }

                                RowLayout {
                                    Layout.preferredWidth: 68
                                    spacing: 4
                                    Rectangle {
                                        width: 6; height: 6; radius: 3
                                        color: modelData.isActive ? "#00FF88" : "#50FFFFFF"
                                    }
                                    Text {
                                        text: modelData.isActive ? qsTr("Active") : qsTr("Idle")
                                        color: modelData.isActive ? "#00FF88" : Theme.fgMuted
                                        font.pixelSize: 11
                                    }
                                }

                                Text {
                                    Layout.preferredWidth: 88
                                    text: modelData.promptTokens + " tok"
                                    color: Theme.fgSecondary
                                    font.pixelSize: 11
                                    font.family: Theme.fontMono
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.preferredWidth: 68
                                    text: modelData.cacheHitRatePct > 0 ? (modelData.cacheHitRatePct.toFixed(1) + "%") : "--"
                                    color: modelData.cacheHitRatePct > 0 ? "#00F0FF" : Theme.fgMuted
                                    font.pixelSize: 11
                                    font.family: Theme.fontMono
                                }

                                Text {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignRight
                                    text: modelData.decodedTokens + " tok"
                                    color: Theme.fgPrimary
                                    font.pixelSize: 11
                                    font.family: Theme.fontMono
                                }
                            }
                        }
                    }

                    // Fallback when server launched without --slots
                    Text {
                        visible: (telemetryService && telemetryService.slotsDisabled)
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("(Server started without --slots; running in basic mode)")
                        color: Theme.fgMuted
                        font.pixelSize: Theme.textXs
                        font.italic: true
                    }
                }
            }

            // 5. Desktop Pet Status Banner
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                implicitHeight: 38
                height: 38
                radius: Theme.radiusMd
                color: "#15FFFFFF"
                border.color: "#25FFFFFF"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    spacing: Theme.space2

                    Text { text: "🦙"; font.pixelSize: 16 }
                    Text {
                        text: root.formatDialogText()
                        color: Theme.fgPrimary
                        font.pixelSize: Theme.textXs
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            // 6. Action Buttons Row
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space3

                MButton {
                    text: qsTr("Copy Restart Command")
                    iconSource: "qrc:/icons/icon-export.svg"
                    Layout.fillWidth: true
                    onClicked: {
                        if (typeof llamapet !== "undefined" && llamapet) {
                            llamapet.copyRestartCommand();
                        }
                    }
                }

                MButton {
                    text: qsTr("Clear Idle KV Cache")
                    variant: MButton.Variant.Primary
                    Layout.fillWidth: true
                    enabled: telemetryService ? !telemetryService.slotsDisabled : true
                    onClicked: {
                        if (typeof llamapet !== "undefined" && llamapet) {
                            llamapet.clearIdleKv();
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
