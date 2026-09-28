#include "ui/controllers/EngineController.h"

#include <QMetaObject>
#include <vector>
#include <QQuickWindow>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "effects/EffectContext.h"
#include "effects/beauty/BeautyProvider.h"

#ifdef Q_OS_WIN
#include "graphics/compositor/Compositor.h"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
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

    HAOCAM_LOG_INFO(kCategory, "attach: render thread handed over the device");

#ifdef Q_OS_WIN
    // IMPORTANT: this runs on the scene-graph RENDER thread. Effect-pipeline
    // init (D3D11 objects, shader compile) and camera bootstrap (Media
    // Foundation enumeration) are heavy - run them on a dedicated attach
    // thread so the render thread returns immediately (the window can paint
    // while the engine warms up; observed a GUI freeze when this chain ran
    // synchronously on the render path).
    const auto* device = d3d11Device; // used to pin the engine to Qt's adapter
    const auto* context = d3d11Context;
    if (m_attachThread.joinable()) m_attachThread.join();
    m_attachThread = std::thread([this, device, context] {
        core::setThreadName("haocam-attach");
        (void)context;

        // ENGINE-OWNED DEVICE (2026-09-28): sharing the Qt render device with
        // ANY second thread wedged the NVIDIA driver stack on the test machine
        // (first Media Foundation, then our own engine-thread uploads - both
        // deadlocked d3d11.dll with 0% CPU). The engine therefore runs on its
        // own D3D11 device; the final frame crosses to the render device as a
        // D3D11 shared resource (see VideoView import).
        // The engine device is created on the RENDER device's DXGI adapter:
        // plain MISC_SHARED textures cannot cross adapters (e.g. Qt on the
        // iGPU while the engine landed on the dGPU -> OpenSharedResource
        // would fail and the preview would stay black).
        Microsoft::WRL::ComPtr<IDXGIAdapter> renderAdapter;
        if (device) {
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
            if (SUCCEEDED(static_cast<ID3D11Device*>(const_cast<void*>(device))
                              ->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
                dxgiDevice->GetAdapter(&renderAdapter);
            }
        }
        Microsoft::WRL::ComPtr<ID3D11Device> engineDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> engineContext;
        const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        };
        HRESULT hr = E_FAIL;
        if (renderAdapter) {
            hr = D3D11CreateDevice(
                renderAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, engineDevice.GetAddressOf(), nullptr,
                engineContext.GetAddressOf());
            if (SUCCEEDED(hr)) {
                HAOCAM_LOG_INFO(kCategory,
                                "Engine device created on the render device's DXGI "
                                "adapter (same-adapter sharing)");
            }
        }
        if (FAILED(hr)) {
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, engineDevice.GetAddressOf(), nullptr,
                engineContext.GetAddressOf());
        }
        if (FAILED(hr)) {
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, engineDevice.GetAddressOf(), nullptr,
                engineContext.GetAddressOf());
            if (SUCCEEDED(hr)) {
                HAOCAM_LOG_WARN(kCategory, "Engine GPU running on WARP software device");
            }
        }
        if (FAILED(hr)) {
            m_started = false;
            QMetaObject::invokeMethod(
                this,
                [this] { emit engineFailed("Engine D3D11 device creation failed"); },
                Qt::QueuedConnection);
            return;
        }

        // Process-lifetime keepalives: EffectContext carries raw pointers and
        // the pipeline uses them until shutdown.
        static Microsoft::WRL::ComPtr<ID3D11Device> s_deviceKeepalive;
        static Microsoft::WRL::ComPtr<ID3D11DeviceContext> s_contextKeepalive;
        s_deviceKeepalive = engineDevice;
        s_contextKeepalive = engineContext;

        HAOCAM_LOG_INFO(kCategory,
                        "Engine owns its own D3D11 device; output shared to Qt "
                        "via D3D11 shared resources");
        EffectContext attachContext;
        attachContext.device = engineDevice.Get();
        attachContext.context = engineContext.Get();
        attachContext.crossDeviceOutput = true;

        HAOCAM_LOG_INFO(kCategory, "attach-async: stage=pipeline-init");
        if (!m_effects->initialize(attachContext)) {
            m_started = false;
            QMetaObject::invokeMethod(
                this,
                [this] { emit engineFailed("GPU pipeline initialization failed"); },
                Qt::QueuedConnection);
            return;
        }

        m_started = true;
        HAOCAM_LOG_INFO(kCategory, "attach-async: stage=engine-thread");
        startEngineThread();
        HAOCAM_LOG_INFO(kCategory, "attach-async: stage=camera");
        // NOTE: the render device is NOT passed to the camera. Media
        // Foundation on the render device deadlocked the driver (mfplat vs
        // d3d11.dll, stacks 2026-09-28). Camera frames arrive as CPU NV12 and
        // are uploaded by the engine thread instead.
        startCamera();
        HAOCAM_LOG_INFO(kCategory, "Engine attached to Qt Quick D3D11 device");

        QMetaObject::invokeMethod(
            this,
            [this] {
                emit activeStagesChanged();
                emit providerStatusChanged();
                emit engineStarted();
            },
            Qt::QueuedConnection);
    });
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
    const std::vector<CameraDevice> devices = m_camera->enumerateDevices();
    // A persisted deviceId can go stale (virtual cam removed, USB instance
    // path changed after a replug/reboot). Only reuse it if it still
    // enumerates; otherwise fall back to auto-pick (physical first).
    bool lastStillPresent = false;
    for (const auto& device : devices) {
        if (device.id == lastDevice) {
            lastStillPresent = true;
            break;
        }
    }
    if (!lastDevice.empty() && lastStillPresent) {
        m_camera->start(lastDevice);
    } else {
        if (!lastDevice.empty()) {
            HAOCAM_LOG_INFO(kCategory,
                            "Persisted deviceId no longer present; using auto-select");
        }
        m_camera->start();
    }
}

void EngineController::shutdownEngine() {
    if (m_attachThread.joinable()) {
        // The attach thread owns startEngineThread()/startCamera(); let it
        // finish before tearing the pipeline down.
        m_attachThread.join();
    }
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

bool EngineController::attachToPipeline(::QQuickWindow*, void* d3d11Device,
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
