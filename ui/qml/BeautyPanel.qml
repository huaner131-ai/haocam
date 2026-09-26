// Beauty controls (Phase 2, spec sections 25-27).
// Values are application-normalized 0.0..1.0; the Facebetter adapter maps
// them to SDK semantics. When the SDK is unavailable the panel shows the
// honest status message and disables the sliders (preview keeps running).

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

    component BeautySlider : ColumnLayout {
        property string label: ""
        property real value: 0
        property var apply: function(v) {}

        spacing: 2
        Layout.preferredWidth: 190

        RowLayout {
            Layout.fillWidth: true
            Label { text: sliderLabel; color: root.textDim; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Label {
                text: Math.round(sliderValue * 100) + "%"
                color: sliderValue > 0 ? root.accent : root.textDim
                font.pixelSize: 11
            }
        }
        Slider {
            id: slider
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            stepSize: 0.01
            value: sliderValue
            enabled: BeautyController.available
            opacity: enabled ? 1.0 : 0.45
            onMoved: sliderApply(slider.value)
        }

        property string sliderLabel: label
        property real sliderValue: value
        property var sliderApply: apply
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 36

        // ---- Skin group (spec section 25) ----
        ColumnLayout {
            spacing: 8
            Label { text: "SKIN"; color: root.textDim; font.pixelSize: 11; font.bold: true }
            BeautySlider { label: "Smoothing"; sliderValue: BeautyController.smoothing;
                           sliderApply: function(v) { BeautyController.smoothing = v } }
            BeautySlider { label: "Whitening"; sliderValue: BeautyController.whitening;
                           sliderApply: function(v) { BeautyController.whitening = v } }
            BeautySlider { label: "Rosy"; sliderValue: BeautyController.rosy;
                           sliderApply: function(v) { BeautyController.rosy = v } }
            BeautySlider { label: "Sharpen"; sliderValue: BeautyController.sharpen;
                           sliderApply: function(v) { BeautyController.sharpen = v } }
        }

        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: root.stroke }

        // ---- Face group ----
        ColumnLayout {
            spacing: 8
            Label { text: "FACE"; color: root.textDim; font.pixelSize: 11; font.bold: true }
            BeautySlider { label: "Face Slim"; sliderValue: BeautyController.faceSlim;
                           sliderApply: function(v) { BeautyController.faceSlim = v } }
            BeautySlider { label: "Eye Size"; sliderValue: BeautyController.eyeSize;
                           sliderApply: function(v) { BeautyController.eyeSize = v } }
            BeautySlider { label: "Nose"; sliderValue: BeautyController.noseSize;
                           sliderApply: function(v) { BeautyController.noseSize = v } }
            BeautySlider { label: "Jaw"; sliderValue: BeautyController.jawSlim;
                           sliderApply: function(v) { BeautyController.jawSlim = v } }
        }

        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: root.stroke }

        // ---- Reset + status (spec sections 26/27) ----
        ColumnLayout {
            spacing: 10
            Layout.maximumWidth: 260

            Button {
                id: resetButton
                text: "Reset Beauty"
                enabled: BeautyController.available
                onClicked: BeautyController.reset()
                Layout.preferredWidth: 140
                background: Rectangle {
                    radius: 8
                    color: resetButton.enabled ? (resetButton.hovered ? root.surfaceAlt : "#141419")
                                               : "#101014"
                    border.color: root.stroke
                }
                contentItem: Label {
                    text: resetButton.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: resetButton.enabled ? root.text : root.textDim
                    font.pixelSize: 12
                }
            }

            RowLayout {
                spacing: 8
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: BeautyController.available ? "#3ddc84"
                         : BeautyController.statusDetail.indexOf("Invalid credentials") === 0 ? "#ff4d4d"
                         : BeautyController.statusDetail.indexOf("initializ") >= 0 ? "#ff8a3d"
                         : "#8b8b98"
                }
                Label { text: "Facebetter"; color: root.text; font.pixelSize: 12; font.bold: true }
            }
            Label {
                text: BeautyController.available
                      ? "● Ready"
                      : BeautyController.statusDetail
                color: root.textDim
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: 250
            }
            Label {
                visible: !BeautyController.available
                text: "Add the Facebetter SDK and credentials (config.json) to enable Beauty."
                color: root.textDim
                font.pixelSize: 11
                font.italic: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: 250
            }
        }
    }
}
