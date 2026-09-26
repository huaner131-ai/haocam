// Beauty controls - powered by the Facebetter provider (Phase 2).
// Phase 1 shows honest availability status from the provider registry.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HaoCam.Controllers

Rectangle {
    color: "transparent"

    function statusFor(slot) {
        const list = EngineController.providerStatus
        for (let i = 0; i < list.length; ++i) {
            if (list[i].slot === slot) return list[i]
        }
        return { available: false, detail: "unknown" }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8
        width: Math.min(parent.width - 60, 560)

        Label {
            text: "Beauty / Retouching"
            color: root.text
            font.pixelSize: 16
            font.bold: true
        }
        Label {
            text: "Facebetter beauty engine - arriving in Phase 2.\n" +
                  "Smoothing, whitening, rosy, sharpen, face slim, eye size, nose and jaw reshape."
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
                anchors.rightMargin: 12
                spacing: 8
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: statusFor("Beauty").available ? "#3ddc84" : "#8b8b98"
                }
                Label {
                    text: "Facebetter: " + statusFor("Beauty").detail
                    color: root.textDim
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}
