#pragma once

// Developer diagnostics overlay controller (FPS, frame time, GPU time,
// CPU time, camera resolution, active effects, dropped frames).

#include <QAtomicInt>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace haocam::app {

class EngineController;

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
};

} // namespace haocam::app
