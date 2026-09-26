// AR lens browser - powered by the Snap Camera Kit provider (Phase 4).

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HaoCam.Controllers

Rectangle {
    color: "transparent"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8
        width: Math.min(parent.width - 60, 560)

        Label {
            text: "AR Effects"
            color: root.text
            font.pixelSize: 16
            font.bold: true
        }
        Label {
            text: "Snap Camera Kit - arriving in Phase 4 through HaoCam's web runtime.\n" +
                  "Lens browser, favorites, recently used, face AR, particles and masks."
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
                    text: "Snap Camera Kit: Unavailable - SDK not integrated yet (Phase 4, web runtime)."
                    color: root.textDim
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}
