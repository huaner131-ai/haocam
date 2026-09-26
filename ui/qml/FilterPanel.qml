// Native color filter controls (Phase 1: brightness / contrast / saturation
// executed on the GPU in the color pass). LUT filters arrive later.

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

    component FilterSlider : ColumnLayout {
        property string title: ""
        property int value: 0

        spacing: 4
        Layout.preferredWidth: 250

        RowLayout {
            Layout.fillWidth: true
            Label { text: filterTitle; color: root.textDim; font.pixelSize: 11; font.bold: true }
            Item { Layout.fillWidth: true }
            Label { text: filterValue; color: root.text; font.pixelSize: 11 }
        }
        Slider {
            id: slider
            Layout.fillWidth: true
            from: -100
            to: 100
            value: filterValue
            onMoved: filterApply(slider.value)
        }

        property string filterTitle: title
        property int filterValue: value
        property var filterApply: function(v) {}
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 34

        FilterSlider {
            title: "BRIGHTNESS"
            filterTitle: "Brightness"
            filterValue: FilterController.brightness
            filterApply: function(v) { FilterController.brightness = v }
        }
        FilterSlider {
            title: "CONTRAST"
            filterTitle: "Contrast"
            filterValue: FilterController.contrast
            filterApply: function(v) { FilterController.contrast = v }
        }
        FilterSlider {
            title: "SATURATION"
            filterTitle: "Saturation"
            filterValue: FilterController.saturation
            filterApply: function(v) { FilterController.saturation = v }
        }

        Button {
            id: resetButton
            text: "Reset"
            onClicked: FilterController.reset()
            background: Rectangle {
                radius: 8
                color: resetButton.hovered ? root.surfaceAlt : "#141419"
                border.color: root.stroke
            }
            contentItem: Label {
                text: resetButton.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: resetButton.hovered ? root.text : root.textDim
                font.pixelSize: 12
            }
        }
    }
}
