// Makeup controls - powered by the OpenMakeupSDK provider (Phase 3).

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

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8
        width: Math.min(parent.width - 60, 560)

        Label {
            text: "Makeup"
            color: root.text
            font.pixelSize: 16
            font.bold: true
        }
        Label {
            text: "OpenMakeupSDK - arriving in Phase 3 through HaoCam's web runtime.\n" +
                  "Foundation, blush, lipstick, eyeliner, mascara, eyeshadow with color, opacity and finish."
            color: root.textDim
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: 8
            color: root.surfaceAlt
            border.color: root.stroke
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                spacing: 8
                Rectangle { width: 8; height: 8; radius: 4; color: "#8b8b98" }
                Label {
                    text: "OpenMakeupSDK: Unavailable - SDK not integrated yet (Phase 3, web runtime)."
                    color: root.textDim
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}
