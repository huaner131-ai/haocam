#include "ui/controllers/EngineController.h"

#include <QMetaObject>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "effects/EffectContext.h"

#ifdef Q_OS_WIN
#include "graphics/compositor/Compositor.h"
#endif

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam::app {

namespace {
constexpr const char* kCategory = "engine";
}

EngineController::EngineController(QObject* parent)
    : QObject(parent),
      m_queue(std::make_unique<core::FrameQueue<Frame>>(3)),
      m_effects(std::make_unique<EffectManager>()),
      m_camera(std::make_unique<CameraManager>(*m_queue)) {
    // Provider status changes arrive from provider worker threads.
    m_statusToken = core::EventBus::instance().subscribe<events::ProviderStatusChanged>(
        [this](const events::ProviderStatusChanged&) {
            QMetaObject::invokeMethod(this, [this] { refreshProviderStatus(); },
                                      Qt::QueuedConnection);
        });
}

EngineController::~EngineController() {
    if (m_statusToken) {
        core::EventBus::instance().unsubscribe(m_statusToken);
    }
    shutdownEngine();
}

void EngineController::setSettings(std::shared_ptr<core::AppSettings> settings) {
    m_settings = std::move(settings);
    if (m_camera) {
        m_camera->setMirror(m_settings->getBool("camera", "mirror", true));
    }
}

void EngineController::setAppConfig(const core::AppConfig& config) {
    m_appConfig = config;
    m_effects->setBeautySettings(config.facebetter);
    m_effects->setTrackingSettings(config.tracking);
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
    // Phase 2: texturePool/scheduler slots remain reserved for later phases.

    if (!m_effects->initialize(context)) {
        m_started = false;
        QMetaObject::invokeMethod(
            this, [this] { emit engineFailed("GPU pipeline initialization failed"); },
            Qt::QueuedConnection);
        return false;
    }

    m_started = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            emit activeStagesChanged();
            emit providerStatusChanged();
            emit engineStarted();
        },
        Qt::QueuedConnection);
    HAOCAM_LOG_INFO(kCategory, "Engine attached to Qt Quick D3D11 device");

    // Capture shares the same D3D11 device so NV12 frames never cross device
    // boundaries (docs/GPU_PIPELINE.md).
    m_camera->setExternalCaptureDevice(d3d11Device);

    startEngineThread();
    startCamera();
    return true;
#else
    (void)d3d11Device;
    (void)d3d11Context;
    QMetaObject::invokeMethod(
        this,
        [this] { emit engineFailed("Direct3D 11 preview requires Windows (QSG_RHI_BACKEND=d3d11)."); },
        Qt::QueuedConnection);
    return false;
#endif
}

void EngineController::startEngineThread() {
    if (m_engineThreadRunning.exchange(true)) return;
    m_engineThread = std::thread([this] {
        core::setThreadName("haocam-engine");
        HAOCAM_LOG_INFO(kCategory, "Engine thread started");
        while (m_engineThreadRunning.load()) {
            auto frame = m_queue->pop(std::chrono::milliseconds(50));
            if (!frame) continue;
            // Engine pipeline: colorConvert -> [beauty override] -> composite,
            // then tracking + beauty consume the processed texture
            // (docs/GPU_PIPELINE.md, spec sections 13/20).
            m_effects->process(*frame);
        }
        HAOCAM_LOG_INFO(kCategory, "Engine thread stopped");
    });
}

void EngineController::stopEngineThread() {
    if (!m_engineThreadRunning.exchange(false)) return;
    if (m_engineThread.joinable()) m_engineThread.join();
}

void EngineController::startCamera() {
    const std::string lastDevice =
        m_settings ? m_settings->getString("camera", "deviceId", "") : std::string();
    m_camera->enumerateDevices();
    if (!lastDevice.empty()) {
        m_camera->start(lastDevice);
    } else {
        m_camera->start();
    }
}

void EngineController::shutdownEngine() {
    stopEngineThread();
    if (m_camera) m_camera->stop();
    if (m_queue) m_queue->clear();
    if (m_started.exchange(false)) {
        if (m_effects) m_effects->shutdown();
    }
    m_deviceAttached = false;
}

void EngineController::resetBeauty() {
    if (m_effects->beauty()) m_effects->beauty()->reset();
    HAOCAM_LOG_INFO(kCategory, "Beauty parameters reset");
}

::haocam::Compositor* EngineController::compositor() const {
#ifdef Q_OS_WIN
    return m_effects ? m_effects->compositor() : nullptr;
#else
    return nullptr;
#endif
}

bool EngineController::attachToPipeline(QQuickWindow*, void* d3d11Device,
                                        void* d3d11Context) {
    EngineController* engine = sharedInstance();
    if (!engine) return false;
    return engine->attachRenderDevice(d3d11Device, d3d11Context);
}

::haocam::Compositor* EngineController::sharedCompositor() {
    EngineController* engine = sharedInstance();
    return engine ? engine->compositor() : nullptr;
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

void EngineController::refreshProviderStatus() { emit providerStatusChanged(); }

} // namespace haocam::app
