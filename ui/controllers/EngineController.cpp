#include "ui/controllers/EngineController.h"

#include <QMetaObject>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"

#ifdef Q_OS_WIN
#include "graphics/compositor/Compositor.h"
#endif

namespace haocam::app {

namespace {
constexpr const char* kCategory = "engine";
}

EngineController::EngineController(QObject* parent)
    : QObject(parent),
      m_queue(std::make_unique<core::FrameQueue<Frame>>(3)),
      m_effects(std::make_unique<EffectManager>()),
      m_camera(std::make_unique<CameraManager>(*m_queue)) {}

EngineController::~EngineController() { shutdownEngine(); }

void EngineController::setSettings(std::shared_ptr<core::AppSettings> settings) {
    m_settings = std::move(settings);
    if (m_camera) {
        m_camera->setMirror(m_settings->getBool("camera", "mirror", true));
    }
}

bool EngineController::attachRenderDevice(void* d3d11Device, void* d3d11Context) {
    bool expected = false;
    if (!m_deviceAttached.compare_exchange_strong(expected, true)) {
        return m_started.load();
    }

#ifdef Q_OS_WIN
    EffectContext context;
    context.device = static_cast<ID3D11Device*>(d3d11Device);
    context.context = static_cast<ID3D11DeviceContext*>(d3d11Context);
    // Phase 2+: faceTracker / texturePool / scheduler come online here.

    if (!m_effects->initialize(context)) {
        m_failed = true;
        QMetaObject::invokeMethod(this, [this] { emit engineFailed("GPU pipeline initialization failed"); },
                                  Qt::QueuedConnection);
        return false;
    }

    m_started = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            emit activeStagesChanged();
            emit engineStarted();
        },
        Qt::QueuedConnection);
    HAOCAM_LOG_INFO(kCategory, "Engine attached to Qt Quick D3D11 device");

    // Capture shares the same D3D11 device so NV12 frames never cross device
    // boundaries (docs/GPU_PIPELINE.md).
    m_camera->setExternalCaptureDevice(d3d11Device);
    startCamera();
    return true;
#else
    (void)d3d11Device;
    (void)d3d11Context;
    m_failed = true;
    QMetaObject::invokeMethod(
        this,
        [this] { emit engineFailed("Direct3D 11 preview requires Windows (QSG_RHI_BACKEND=d3d11)."); },
        Qt::QueuedConnection);
    return false;
#endif
}

void EngineController::startCamera() {
    const std::string lastDevice =
        m_settings ? m_settings->getString("camera", "deviceId", "") : std::string();
    m_camera->enumerateDevices();
    if (!lastDevice.empty()) {
        // Prefer the last used camera; CameraManager falls back to the first
        // device when it is no longer present.
        m_camera->start(lastDevice);
    } else {
        m_camera->start();
    }
}

void EngineController::shutdownEngine() {
    if (m_camera) m_camera->stop();
    if (m_queue) m_queue->clear();
    if (m_started.exchange(false)) {
        if (m_effects) m_effects->shutdown();
    }
    m_deviceAttached = false;
    HAOCAM_LOG_INFO(kCategory, "Engine shut down");
}

::haocam::Compositor* EngineController::compositor() const {
#ifdef Q_OS_WIN
    return m_effects ? m_effects->compositor() : nullptr;
#else
    return nullptr;
#endif
}

QStringList EngineController::activeStages() const {
    QStringList stages;
    for (const auto& name : m_effects->graph().activeStageNames()) {
        stages << QString::fromStdString(name);
    }
    return stages;
}

QVariantList EngineController::providerStatus() const {
    QVariantList list;
    for (const auto& status : m_effects->providerStatus()) {
        QVariantMap entry;
        entry.insert("slot", QString::fromStdString(status.slot));
        entry.insert("provider", QString::fromStdString(status.provider));
        entry.insert("available", status.available);
        entry.insert("detail", QString::fromStdString(status.detail));
        list.append(entry);
    }
    return list;
}

} // namespace haocam::app
