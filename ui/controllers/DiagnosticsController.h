#pragma once

// Developer diagnostics overlay controller (spec section 34, Phase 2
// extension: tracking + beauty rows).

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "ui/controllers/EngineController.h"

namespace haocam::app {

class DiagnosticsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool overlayVisible READ overlayVisible WRITE setOverlayVisible NOTIFY
                   overlayVisibleChanged)
    Q_PROPERTY(double previewFps READ previewFps NOTIFY statsChanged)
    Q_PROPERTY(double cameraFps READ cameraFps NOTIFY statsChanged)
    Q_PROPERTY(double frameTimeMs READ frameTimeMs NOTIFY statsChanged)
    Q_PROPERTY(double gpuTimeMs READ gpuTimeMs NOTIFY statsChanged)
    Q_PROPERTY(double cpuTimeMs READ cpuTimeMs NOTIFY statsChanged)
    Q_PROPERTY(QString cameraResolution READ cameraResolution NOTIFY statsChanged)
    Q_PROPERTY(QStringList activeEffects READ activeEffects NOTIFY statsChanged)
    Q_PROPERTY(uint64_t droppedFrames READ droppedFrames NOTIFY statsChanged)
    Q_PROPERTY(int pooledTextures READ pooledTextures NOTIFY statsChanged)

    // Phase 2: tracking + beauty.
    Q_PROPERTY(double trackingFps READ trackingFps NOTIFY statsChanged)
    Q_PROPERTY(double trackingMs READ trackingMs NOTIFY statsChanged)
    Q_PROPERTY(float faceConfidence READ faceConfidence NOTIFY statsChanged)
    Q_PROPERTY(int faceCount READ faceCount NOTIFY statsChanged)
    Q_PROPERTY(bool beautyEnabled READ beautyEnabled NOTIFY statsChanged)
    Q_PROPERTY(double beautyMs READ beautyMs NOTIFY statsChanged)
    Q_PROPERTY(QString beautyStatus READ beautyStatus NOTIFY statsChanged)

public:
    explicit DiagnosticsController(EngineController& engine, QObject* parent = nullptr);

    bool overlayVisible() const { return m_overlayVisible; }
    double previewFps() const { return m_previewFps; }
    double cameraFps() const { return m_cameraFps; }
    double frameTimeMs() const { return m_frameTimeMs; }
    double gpuTimeMs() const { return m_gpuTimeMs; }
    double cpuTimeMs() const { return m_cpuTimeMs; }
    QString cameraResolution() const { return m_cameraResolution; }
    QStringList activeEffects() const { return m_activeEffects; }
    uint64_t droppedFrames() const { return m_droppedFrames; }
    int pooledTextures() const { return m_pooledTextures; }

    double trackingFps() const { return m_trackingFps; }
    double trackingMs() const { return m_trackingMs; }
    float faceConfidence() const { return m_faceConfidence; }
    int faceCount() const { return m_faceCount; }
    bool beautyEnabled() const { return m_beautyEnabled; }
    double beautyMs() const { return m_beautyMs; }
    QString beautyStatus() const { return m_beautyStatus; }

    Q_INVOKABLE void toggleOverlay() { setOverlayVisible(!m_overlayVisible); }
    void setOverlayVisible(bool visible);

signals:
    void overlayVisibleChanged();
    void statsChanged();

private:
    void refresh();

    EngineController& m_engine;
    QTimer m_timer;
    bool m_overlayVisible = false;
    double m_previewFps = 0.0;
    double m_cameraFps = 0.0;
    double m_frameTimeMs = 0.0;
    double m_gpuTimeMs = 0.0;
    double m_cpuTimeMs = 0.0;
    QString m_cameraResolution = QStringLiteral("-");
    QStringList m_activeEffects;
    uint64_t m_droppedFrames = 0;
    int m_pooledTextures = 0;
    double m_trackingFps = 0.0;
    double m_trackingMs = 0.0;
    float m_faceConfidence = 0.0f;
    int m_faceCount = 0;
    bool m_beautyEnabled = false;
    double m_beautyMs = 0.0;
    QString m_beautyStatus;
};

} // namespace haocam::app
