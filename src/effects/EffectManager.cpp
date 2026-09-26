#include "effects/EffectManager.h"

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "effects/EffectContext.h"
#include "effects/beauty/BeautyProvider.h"
#include "effects/beauty/NullBeautyProvider.h"

#ifdef HAOCAM_HAS_FACEBETTER
#include "effects/beauty/FacebetterProvider.h"
#endif

#ifdef _WIN32
#include "face/MediaPipePixelSource.h"
#include "graphics/compositor/Compositor.h"
#else
#include <memory>
#endif

namespace haocam {

namespace {
constexpr const char* kCategory = "effects";

// Composite uses the beauty output while it lags by at most this many
// frames; beyond that the frame passes through un-beautified (spec section
// 33: never leave a stale beauty frame on screen).
constexpr uint64_t kBeautyMaxLagFrames = 3;
} // namespace

EffectManager::EffectManager() = default;

EffectManager::~EffectManager() { shutdown(); }

void EffectManager::setBeautySettings(const core::FacebetterSettings& settings) {
    m_beautySettings = settings;
}

void EffectManager::setTrackingSettings(const core::TrackingSettings& settings) {
    m_trackingSettings = settings;
}

FaceTrackerConfig EffectManager::trackingConfig() const {
    FaceTrackerConfig config;
    config.modelAssetPath = m_trackingSettings.modelPath.empty()
                                ? std::string("assets/models/face_landmarker.task")
                                : m_trackingSettings.modelPath;
    config.maxTrackingFps = m_trackingSettings.maxFps;
    config.smoothingMinCutoff = m_trackingSettings.smoothingMinCutoff;
    config.smoothingBeta = m_trackingSettings.smoothingBeta;
    return config;
}

bool EffectManager::initialize(const EffectContext& context) {
    if (m_initialized) return true;

    // ---- Tracking worker (tracker + pixel source + smoothing) ----
    std::unique_ptr<IPixelSource> pixelSource;
#ifdef _WIN32
    pixelSource = D3D11PixelSource::create(context.device, context.context);
#endif
    if (!pixelSource) {
        pixelSource = std::make_unique<CpuNv12PixelSource>();
    }
    m_tracking = std::make_unique<TrackingWorker>(createDefaultTracker(),
                                                  std::move(pixelSource));

    // ---- Beauty provider ----
#ifdef HAOCAM_HAS_FACEBETTER
    auto facebetter = std::make_unique<FacebetterProvider>();
    facebetter->configure(m_beautySettings, context.device, context.texturePool);
    FacebetterProvider* facebetterPtr = facebetter.get();
    m_beauty = std::move(facebetter);
    (void)facebetterPtr;
#else
    if (m_beautySettings.enabled) {
        m_beauty = std::make_unique<NullBeautyProvider>(
            "Unavailable: Facebetter SDK not compiled in (drop the SDK under "
            "sdk/facebetter and build with HAOCAM_ENABLE_FACEBETTER=ON)");
    } else {
        m_beauty = std::make_unique<NullBeautyProvider>(
            "Unavailable: not configured (add credentials to config.json and "
            "set facebetter.enabled=true)");
    }
#endif

    if (auto* nullBeauty = dynamic_cast<NullBeautyProvider*>(m_beauty.get())) {
        (void)nullBeauty; // status text is carried by the provider itself
    }

    // ---- GPU compositor (Windows) ----
#ifdef _WIN32
    m_compositor = std::make_unique<Compositor>();
    if (!m_compositor->initialize(context.device, context.context)) {
        HAOCAM_LOG_ERROR(kCategory, "GPU compositor failed to initialize; preview disabled");
        m_compositor.reset();
    }
#endif

    // ---- Graph stages (Camera -> Tracking -> Beauty -> Composite -> Out) ----
    m_graph.setStageEnabled(EffectStage::CameraCapture, true);
    m_graph.setStageEnabled(EffectStage::ColorConversion, true);
    m_graph.setStageEnabled(EffectStage::FaceTracking,
                            m_trackingSettings.enabled &&
                                m_tracking->start(trackingConfig()));
    m_graph.setStageEnabled(EffectStage::Beauty, m_beauty != nullptr &&
                                                    m_beautySettings.enabled);
    m_graph.setStageEnabled(EffectStage::FinalComposite, true);
    m_graph.setStageEnabled(EffectStage::Outputs, true);

    m_initialized = true;
    HAOCAM_LOG_INFO(kCategory,
                    "Effect pipeline initialized (tracking={}, beauty={} / {})",
                    m_graph.isStageEnabled(EffectStage::FaceTracking),
                    m_graph.isStageEnabled(EffectStage::Beauty),
                    m_beauty->statusText());
    return true;
}

void EffectManager::shutdown() {
    if (!m_initialized) return;
    if (m_tracking) {
        m_tracking->stop();
        m_tracking.reset();
    }
    if (m_beauty) {
        m_beauty->shutdown();
        m_beauty.reset();
    }
#ifdef _WIN32
    if (m_compositor) {
        m_compositor->shutdown();
        m_compositor.reset();
    }
#endif
    m_initialized = false;
    HAOCAM_LOG_INFO(kCategory, "Effect pipeline shut down");
}

IBeautyProvider* EffectManager::beauty() { return m_beauty.get(); }

IFaceTracker* EffectManager::faceTracker() {
    return m_tracking ? m_tracking->tracker() : nullptr;
}

Compositor* EffectManager::compositor() const {
#ifdef _WIN32
    return m_compositor.get();
#else
    return nullptr;
#endif
}

void EffectManager::updateTracking(const Frame& frame) {
    if (!m_tracking) return;
    TrackingInput input;
    if (frame.isGpu()) {
        input.gpu = frame.texture.ref;
    } else {
        input.cpu = frame.cpuBuffer;
    }
    input.width = frame.width;
    input.height = frame.height;
    input.frameId = frame.id;
    input.timestampUs = frame.timestamp;
    m_tracking->submit(std::move(input));
    m_latestTracking = m_tracking->latestTracking();
}

void EffectManager::process(Frame& frame) {
    if (!frame.isValid()) return;

#ifdef _WIN32
    if (!m_compositor) return;

    // Beauty override (freshness-gated).
    GpuTextureRef beautyOutput;
    uint64_t beautyFrameId = 0;
    if (m_beauty && m_beauty->isAvailable()) {
        if (auto* facebetter = dynamic_cast<IBeautyAsync*>(m_beauty.get())) {
            if (facebetter->latestOutput(beautyOutput, beautyFrameId) &&
                beautyFrameId + kBeautyMaxLagFrames >= frame.id) {
                m_compositor->setCompositeOverride(beautyOutput);
                m_lastBeautyFrameId = beautyFrameId;
            } else {
                m_compositor->clearCompositeOverride();
            }
        } else {
            m_compositor->clearCompositeOverride();
        }
    } else {
        m_compositor->clearCompositeOverride();
    }

    m_compositor->process(frame);

    // Feed tracking + beauty with the processed (un-beautified) texture.
    if (m_tracking && m_graph.isStageEnabled(EffectStage::FaceTracking)) {
        Frame processedShell;
        processedShell.id = frame.id;
        processedShell.timestamp = frame.timestamp;
        processedShell.width = frame.width;
        processedShell.height = frame.height;
        processedShell.texture.ref = m_compositor->lastProcessedTexture();
        updateTracking(processedShell);
    }
    if (m_beauty && m_beauty->isAvailable() &&
        m_graph.isStageEnabled(EffectStage::Beauty)) {
        if (auto* async = dynamic_cast<IBeautyAsync*>(m_beauty.get())) {
            async->submitFrame(m_compositor->lastProcessedTexture(), frame.id);
        }
    }
#else
    (void)frame;
#endif
}

void EffectManager::reset() {
    if (m_beauty) m_beauty->reset();
    if (m_tracking && m_tracking->tracker()) {
        // Tracker reset = smoothing reset only (stateless detector).
    }
    HAOCAM_LOG_DEBUG(kCategory, "Effect pipeline reset");
}

std::vector<EffectManager::ProviderStatus> EffectManager::providerStatus() const {
    std::vector<ProviderStatus> status;
    if (m_beauty) {
        status.push_back({"Beauty", std::string(m_beauty->name()), m_beauty->isAvailable(),
                          m_beauty->statusText()});
    } else {
        status.push_back({"Beauty", "Facebetter", false, "Unavailable"});
    }
    if (m_tracking) {
        const bool trackingOn = m_graph.isStageEnabled(EffectStage::FaceTracking);
        status.push_back({"Tracking", std::string("MediaPipe"), trackingOn,
                          trackingOn ? m_tracking->trackerStatus()
                                     : std::string("Disabled in config.json")});
    }
    status.push_back({"Makeup", "OpenMakeupSDK", false,
                      "Unavailable: SDK integration arrives in Phase 3 (web runtime)."});
    status.push_back({"AR", "Snap Camera Kit", false,
                      "Unavailable: SDK integration arrives in Phase 4 (web runtime)."});
    status.push_back({"Native", "HaoCam GPU passes", true, "Active."});
    return status;
}

EffectManager::BeautyDiagnostics EffectManager::beautyDiagnostics() const {
    BeautyDiagnostics diag;
    diag.enabled = m_graph.isStageEnabled(EffectStage::Beauty) && m_beauty != nullptr;
    diag.available = m_beauty && m_beauty->isAvailable();
    if (m_beauty) {
        diag.readbackMs = m_beauty->lastReadbackMs();
        diag.sdkProcessMs = m_beauty->lastSdkProcessMs();
        diag.uploadMs = m_beauty->lastUploadMs();
        diag.detectedFaces = m_beauty->detectedFaceCount();
        diag.processedFrames = m_beauty->processedFrames();
    }
    return diag;
}

FaceTrackingResult EffectManager::latestTracking() const {
    return m_tracking ? m_tracking->latestTracking() : FaceTrackingResult{};
}

} // namespace haocam
