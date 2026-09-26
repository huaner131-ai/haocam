// Recording controls - FFmpeg backend arrives in Phase 5.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    color: "transparent"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8
        width: Math.min(parent.width - 60, 560)

        Label {
            text: "Recording"
            color: root.text
            font.pixelSize: 16
            font.bold: true
        }
        Label {
            text: "FFmpeg H.264/H.265 MP4 recording of the FINAL processed frame - arriving in Phase 5.\n" +
                  "Recording consumes the same GPU texture as the preview (no second pipeline)."
            color: root.textDim
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Button {
            enabled: false
            text: "Start Recording"
            Layout.preferredWidth: 160
            background: Rectangle {
                radius: 8
                color: "#141419"
                border.color: root.stroke
            }
            contentItem: Label {
                text: "Start Recording (Phase 5)"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: root.textDim
                font.pixelSize: 12
            }
        }
    }
}
