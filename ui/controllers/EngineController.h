#pragma once

// Engine-facing controller: owns the core engine objects (frame queue,
// camera manager, effect manager) and exposes engine status to QML.
//
// Phase 2: runs the ENGINE THREAD (spec section 13):
//   Camera thread -> FrameQueue(3) -> engine thread
//     -> EffectManager::process (colorConvert -> [beauty] -> composite)
//     -> tracking + beauty fed with the processed texture
// The queue is bounded and drop-oldest; the engine thread is the single
// consumer, so queue growth is impossible by construction.

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include "camera/CameraManager.h"
#include "core/config/AppConfig.h"
#include "core/threading/FrameQueue.h"
#include "effects/EffectManager.h"

namespace haocam {
class Compositor; // graphics/compositor/Compositor.h (Windows/D3D11 builds)
} // namespace haocam

namespace haocam::app {

class EngineController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList activeStages READ activeStages NOTIFY activeStagesChanged)
    Q_PROPERTY(QVariantList providerStatus READ providerStatus NOTIFY providerStatusChanged)

public:
    explicit EngineController(QObject* parent = nullptr);
    ~EngineController() override;

    // Shared instance used by VideoView (render thread access).
    static EngineController* sharedInstance() { return s_instance; }
    static void setSharedInstance(EngineController* instance) { s_instance = instance; }

    // Render-thread entry point: fetches Qt Quick's D3D11 device and starts
    // the engine (Windows).
    static bool attachToPipeline(class QQuickWindow* window, void* d3d11Device,
                                 void* d3d11Context);
    static ::haocam::Compositor* sharedCompositor();

    void setSettings(std::shared_ptr<core::AppSettings> settings);
    void setAppConfig(const core::AppConfig& config);
    core::AppSettings* settings() const { return m_settings.get(); }
    const core::AppConfig& appConfig() const { return m_appConfig; }

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
    Q_INVOKABLE void resetBeauty();

signals:
    void activeStagesChanged();
    void providerStatusChanged();
    void engineStarted();
    void engineFailed(QString reason);

private:
    void startCamera();
    void startEngineThread();
    void stopEngineThread();
    void refreshProviderStatus();

    std::shared_ptr<core::AppSettings> m_settings;
    core::AppConfig m_appConfig;
    std::unique_ptr<core::FrameQueue<Frame>> m_queue;
    std::unique_ptr<EffectManager> m_effects;
    std::unique_ptr<CameraManager> m_camera;

    std::thread m_engineThread;
    std::atomic<bool> m_engineThreadRunning{false};
    std::atomic<bool> m_deviceAttached{false};
    std::atomic<bool> m_started{false};

    core::EventBus::Token m_statusToken = 0;

    inline static EngineController* s_instance = nullptr;
};

} // namespace haocam::app
