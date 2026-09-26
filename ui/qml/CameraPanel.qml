// Camera controls: device selection, resolution, mirror.

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
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        anchors.leftMargin: 26
        anchors.rightMargin: 26
        spacing: 22

        // ---- Device ----
        ColumnLayout {
            spacing: 6
            Label { text: "CAMERA"; color: root.textDim; font.pixelSize: 11; font.bold: true }
            ComboBox {
                id: deviceBox
                Layout.preferredWidth: 320
                model: CameraController.devices
                textRole: "name"
                currentValue: CameraController.selectedDeviceId
                onActivated: function(index) {
                    CameraController.selectDevice(CameraController.devices[index]["id"])
                }
                background: Rectangle {
                    radius: 8
                    color: root.surfaceAlt
                    border.color: root.stroke
                }
            }
            Label {
                text: "State: " + CameraController.state
                color: root.textDim
                font.pixelSize: 11
            }
        }

        // ---- Resolution ----
        ColumnLayout {
            spacing: 6
            Label { text: "RESOLUTION"; color: root.textDim; font.pixelSize: 11; font.bold: true }
            RowLayout {
                spacing: 8
                Repeater {
                    model: [
                        { "label": "720p60",  "w": 1280, "h": 720,  "fps": 60 },
                        { "label": "1080p30", "w": 1920, "h": 1080, "fps": 30 },
                        { "label": "1080p60", "w": 1920, "h": 1080, "fps": 60 },
                        { "label": "1440p30", "w": 2560, "h": 1440, "fps": 30 }
                    ]
                    Button {
                        id: resButton
                        required property var modelData
                        text: modelData.label
                        onClicked: CameraController.applyResolution(
                                       modelData.w, modelData.h, modelData.fps)
                        background: Rectangle {
                            radius: 8
                            color: resButton.hovered ? root.surfaceAlt : "#141419"
                            border.color: root.stroke
                        }
                        contentItem: Label {
                            text: resButton.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: resButton.hovered ? root.text : root.textDim
                            font.pixelSize: 12
                        }
                    }
                }
            }
            Label {
                text: "Active: " + CameraController.activeFormat
                color: root.textDim
                font.pixelSize: 11
            }
        }

        Item { Layout.fillWidth: true }

        // ---- Mirror ----
        ColumnLayout {
            spacing: 6
            Label { text: "MIRROR PREVIEW"; color: root.textDim; font.pixelSize: 11; font.bold: true }
            Switch {
                checked: CameraController.mirror
                onCheckedChanged: CameraController.mirror = checked
            }
        }
    }
}
