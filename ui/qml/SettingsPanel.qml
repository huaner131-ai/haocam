// Settings overlay: provider status, diagnostics, about.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HaoCam.Controllers

Rectangle {
    id: root

    readonly property color bg: "#101014"
    readonly property color surface: "#17171d"
    readonly property color surfaceAlt: "#1d1d25"
    readonly property color stroke: "#2a2a33"
    readonly property color text: "#e8e8ee"
    readonly property color textDim: "#8b8b98"
    readonly property color accent: "#ff4d79"
    readonly property color accentAlt: "#4dd8ff"
    id: settingsRoot

    signal close()

    color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Settings"
                color: root.text
                font.pixelSize: 16
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Close"
                onClicked: settingsRoot.close()
                background: Rectangle {
                    radius: 8
                    color: parent.parent.hovered ? root.surfaceAlt : "#141419"
                    border.color: root.stroke
                }
                contentItem: Label {
                    text: "Close"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: root.textDim
                    font.pixelSize: 12
                }
            }
        }

        Label {
            text: "EFFECT PROVIDERS"
            color: root.textDim
            font.pixelSize: 11
            font.bold: true
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: EngineController.providerStatus
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 44
                radius: 8
                color: root.surfaceAlt
                border.color: root.stroke
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: modelData.available ? "#3ddc84" : "#8b8b98"
                    }
                    Label {
                        text: modelData.slot + " - " + modelData.provider
                        color: root.text
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Label {
                        text: modelData.detail
                        color: root.textDim
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Switch {
                id: diagSwitch
                checked: DiagnosticsController.overlayVisible
                onCheckedChanged: if (checked !== DiagnosticsController.overlayVisible)
                                      DiagnosticsController.toggleOverlay()
            }
            Label {
                text: "Show diagnostics overlay (F3)"
                color: root.textDim
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "HaoCam 0.1.0 - Phase 1 preview"
                color: root.textDim
                font.pixelSize: 11
            }
        }
    }
}
