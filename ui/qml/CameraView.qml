// Camera preview area: GPU VideoView + status overlays + diagnostics HUD.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HaoCam
import HaoCam.Controllers

Rectangle {
    id: previewRoot

    required property bool settingsVisible
    signal closedSettings()

    color: "#000000"

    // GPU-composited final frame.
    VideoView {
        id: video
        anchors.fill: parent

        onPreviewFailed: function(reason) {
            previewRoot.statusBanner.visible = true
            previewRoot.statusBannerText = reason
        }
    }

    // ---- Camera status banner (disconnect / no device / starting) ----
    property string statusBannerText: ""
    Rectangle {
        id: statusBanner
        visible: CameraController.state !== "Running" &&
                 CameraController.state !== "Idle"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 18
        radius: 8
        width: bannerRow.implicitWidth + 28
        height: 34
        color: "#cc17171d"
        border.color: previewRoot.statusBannerText.length > 0 ? "#c85a2f" : root.accent
        border.width: 1

        RowLayout {
            id: bannerRow
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                width: 8; height: 8; radius: 4
                color: CameraController.state === "Reconnecting" ? "#ffb020"
                     : CameraController.state === "Failed" ? "#ff4d4d"
                     : CameraController.state === "NoDevice" ? "#8b8b98"
                     : root.accentAlt
            }
            Label {
                text: {
                    const st = CameraController.state
                    if (st === "Reconnecting") return "Camera reconnecting..."
                    if (st === "NoDevice") return "No camera found - plug in a webcam, or use the Camera tab"
                    if (st === "Failed") return "Camera failed: " + CameraController.stateDetail
                    if (st === "Starting") return "Starting camera..."
                    return st
                }
                color: root.text
                font.pixelSize: 12
            }
        }
    }

    // ---- Settings overlay ----
    Rectangle {
        visible: previewRoot.settingsVisible
        anchors.fill: parent
        anchors.margins: 26
        radius: 12
        color: "#f217171d"
        border.color: root.stroke
        border.width: 1

        SettingsPanel {
            anchors.fill: parent
            anchors.margins: 16
            onClose: previewRoot.closedSettings()
        }
    }

    // ---- Diagnostics HUD (developer toggle: Diagnostics button / F3) ----
    Rectangle {
        visible: DiagnosticsController.overlayVisible
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 14
        width: diagColumn.implicitWidth + 24
        height: diagColumn.implicitHeight + 20
        radius: 8
        color: "#d0101014"
        border.color: root.stroke
        border.width: 1

        Shortcut {
            sequence: "F3"
            context: Qt.ApplicationShortcut
            onActivated: DiagnosticsController.toggleOverlay()
        }

        ColumnLayout {
            id: diagColumn
            anchors.centerIn: parent
            spacing: 2

            Label { color: root.accentAlt; font.pixelSize: 11; font.family: "Consolas"
                    text: "fps      " + DiagnosticsController.previewFps.toFixed(1) +
                          "  (cam " + DiagnosticsController.cameraFps.toFixed(1) + ")" }
            Label { color: root.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "frame    " + DiagnosticsController.frameTimeMs.toFixed(2) + " ms" }
            Label { color: root.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "gpu      " + DiagnosticsController.gpuTimeMs.toFixed(2) + " ms" }
            Label { color: root.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "cpu      " + DiagnosticsController.cpuTimeMs.toFixed(2) + " ms" }
            Label { color: root.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "camera   " + DiagnosticsController.cameraResolution }
            Label { color: root.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "stages   " + DiagnosticsController.activeEffects.join(", ") }
            Label { color: root.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "dropped  " + DiagnosticsController.droppedFrames +
                          "   pool " + DiagnosticsController.pooledTextures }
        }
    }
}
