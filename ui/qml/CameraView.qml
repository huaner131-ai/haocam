// Camera preview area: GPU VideoView + status overlays + diagnostics HUD.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import HaoCam
import HaoCam.Controllers

Rectangle {
    // Panel palette (mirrors Main.qml; id stays previewRoot).
    readonly property color bg: "#101014"
    readonly property color surface: "#17171d"
    readonly property color surfaceAlt: "#1d1d25"
    readonly property color stroke: "#2a2a33"
    readonly property color text: "#e8e8ee"
    readonly property color textDim: "#8b8b98"
    readonly property color accent: "#ff4d79"
    readonly property color accentAlt: "#4dd8ff"

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
        border.color: previewRoot.statusBannerText.length > 0 ? "#c85a2f" : previewRoot.accent
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
                     : previewRoot.accentAlt
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
                color: previewRoot.text
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
        border.color: previewRoot.stroke
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
        border.color: previewRoot.stroke
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

            Label { color: previewRoot.accentAlt; font.pixelSize: 11; font.family: "Consolas"
                    text: "fps      " + DiagnosticsController.previewFps.toFixed(1) +
                          "  (cam " + DiagnosticsController.cameraFps.toFixed(1) + ")" }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "frame    " + DiagnosticsController.frameTimeMs.toFixed(2) + " ms" }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "gpu      " + DiagnosticsController.gpuTimeMs.toFixed(2) + " ms" }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "cpu      " + DiagnosticsController.cpuTimeMs.toFixed(2) + " ms" }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "camera   " + DiagnosticsController.cameraResolution }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "stages   " + DiagnosticsController.activeEffects.join(", ") }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "dropped  " + DiagnosticsController.droppedFrames +
                          "   pool " + DiagnosticsController.pooledTextures }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "track    " + DiagnosticsController.trackingFps.toFixed(1) + " fps  " +
                          DiagnosticsController.trackingMs.toFixed(1) + " ms" }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "face     conf " + DiagnosticsController.faceConfidence.toFixed(2) +
                          "  n " + DiagnosticsController.faceCount }
            Label { color: previewRoot.textDim; font.pixelSize: 11; font.family: "Consolas"
                    text: "beauty   " + (DiagnosticsController.beautyEnabled ? "ON" : "OFF") +
                          "  " + DiagnosticsController.beautyMs.toFixed(1) + " ms" }
        }
    }
}
