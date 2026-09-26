#pragma once

// Engine-facing controller: owns the core engine objects (frame queue,
// camera manager, effect manager) and exposes engine status to QML.
// The render-side glue (VideoView) attaches the D3D11 device here.

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <atomic>
#include <cstdint>
#include <memory>

#include "camera/CameraManager.h"
#include "core/threading/FrameQueue.h"
#include "effects/EffectManager.h"

namespace haocam {
class Compositor; // graphics/compositor/Compositor.h (Windows/D3D11 builds)
} // namespace haocam

namespace haocam::app {

class EngineController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList activeStages READ activeStages NOTIFY activeStagesChanged)
    Q_PROPERTY(QVariantList providerStatus READ providerStatus CONSTANT)

public:
    explicit EngineController(QObject* parent = nullptr);
    ~EngineController() override;

    void setSettings(std::shared_ptr<core::AppSettings> settings);
    core::AppSettings* settings() const { return m_settings.get(); }

    CameraManager& cameraManager() { return *m_camera; }
    EffectManager& effectManager() { return *m_effects; }

    // Called by VideoView (render thread) once the Qt Quick D3D11 device is
    // available. Returns false when the engine could not start. Device
    // pointers are raw ID3D11Device*/ID3D11DeviceContext* (void* to keep
    // this header platform-light).
    bool attachRenderDevice(void* d3d11Device, void* d3d11Context);

    // Compositor handle (graphics/compositor/Compositor.h on Windows).
    ::haocam::Compositor* compositor() const;

    // Frames dropped between camera and engine (full queue = slow consumer).
    uint64_t queueDroppedFrames() const { return m_queue ? m_queue->droppedCount() : 0; }

    QStringList activeStages() const;
    QVariantList providerStatus() const;

    Q_INVOKABLE void shutdownEngine();

signals:
    void activeStagesChanged();
    void engineStarted();
    void engineFailed(QString reason);

private:
    void startCamera();

    std::shared_ptr<core::AppSettings> m_settings;
    std::unique_ptr<core::FrameQueue<Frame>> m_queue;
    std::unique_ptr<EffectManager> m_effects;
    std::unique_ptr<CameraManager> m_camera;
    std::atomic<bool> m_deviceAttached{false};
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_failed{false};

    inline static EngineController* s_instance = nullptr;
};

} // namespace haocam::app
