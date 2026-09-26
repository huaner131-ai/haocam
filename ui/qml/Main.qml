// HaoCam main window (dark, creator-focused, preview-first).

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HaoCam
import HaoCam.Controllers

ApplicationWindow {
    id: root

    readonly property color bg: "#101014"
    readonly property color surface: "#17171d"
    readonly property color surfaceAlt: "#1d1d25"
    readonly property color stroke: "#2a2a33"
    readonly property color text: "#e8e8ee"
    readonly property color textDim: "#8b8b98"
    readonly property color accent: "#ff4d79"
    readonly property color accentAlt: "#4dd8ff"

    width: 1280
    minimumWidth: 980
    height: 800
    minimumHeight: 640
    visible: true
    title: "HAO CAM"
    color: bg

    FontLoader { id: uiFont; family: "Segoe UI" }
    font.family: uiFont.name
    font.pixelSize: 13

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Title bar ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 10

                Rectangle {
                    width: 26; height: 26; radius: 8
                    color: root.accent
                    Label {
                        anchors.centerIn: parent
                        text: "HC"
                        color: "white"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
                Label {
                    text: "HAO CAM"
                    color: root.text
                    font.pixelSize: 15
                    font.bold: true
                    font.letterSpacing: 2
                }
                Item { Layout.fillWidth: true }

                ToolButton {
                    id: diagButton
                    checkable: true
                    checked: DiagnosticsController.overlayVisible
                    text: "Diagnostics"
                    onClicked: DiagnosticsController.toggleOverlay()
                    contentItem: Label {
                        text: diagButton.text
                        color: diagButton.checked ? root.accentAlt : root.textDim
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ToolButton {
                    id: settingsButton
                    checkable: true
                    text: "Settings"
                    onClicked: root.settingsVisible = checked
                    contentItem: Label {
                        text: "Settings"
                        color: settingsButton.checked ? root.text : root.textDim
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // ---- Preview ----
        CameraView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            settingsVisible: root.settingsVisible
            onClosedSettings: root.settingsVisible = false
        }

        // ---- Bottom tab bar + controls ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 236
            color: root.surface

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    background: Rectangle { color: "transparent" }

                    Repeater {
                        model: ["Camera", "Beauty", "Makeup", "AR", "Filters", "Record"]
                        TabButton {
                            id: tabButton
                            required property var modelData
                            required property int index
                            text: modelData
                            width: 110
                            checked: tabBar.currentIndex === index
                            contentItem: Label {
                                text: tabButton.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: tabButton.checked ? root.text : root.textDim
                                font.pixelSize: 13
                                font.bold: tabButton.checked
                            }
                            background: Rectangle {
                                color: "transparent"
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottom: parent.bottom
                                    width: 56
                                    height: 3
                                    radius: 2
                                    color: tabButton.checked ? root.accent : "transparent"
                                }
                            }
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabBar.currentIndex

                    CameraPanel {}
                    BeautyPanel {}
                    MakeupPanel {}
                    ARPanel {}
                    FilterPanel {}
                    RecordingPanel {}
                }
            }
        }
    }

    property bool settingsVisible: false
}
