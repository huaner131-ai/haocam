#include "effects/EffectManager.h"

#include <atomic>
#include <memory>

#include "core/logging/Logger.h"
#include "effects/EffectContext.h"
#include "face/FaceTracker.h"
#include "frame/Frame.h"

#ifdef _WIN32
#include "graphics/compositor/Compositor.h"
#endif

namespace haocam {

namespace {
constexpr const char* kCategory = "effects";
}

class EffectManager::Impl {
public:
    std::unique_ptr<IFaceTracker> tracker;
#ifdef _WIN32
    std::unique_ptr<Compositor> compositor;
#endif
    bool initialized = false;
};

EffectManager::EffectManager() : m_impl(std::make_unique<Impl>()) {}

EffectManager::~EffectManager() { shutdown(); }

bool EffectManager::initialize(const EffectContext& context) {
    if (m_impl->initialized) return true;

    // Face tracker (shared by all providers so the frame is processed once).
    m_impl->tracker = createDefaultTracker();
    if (context.faceTracker) {
        // External tracker was supplied (e.g. provider-embedded tracking).
        m_impl->tracker.reset();
        m_graph.setStageEnabled(EffectStage::FaceTracking,
                                context.faceTracker->isAvailable());
        HAOCAM_LOG_INFO(kCategory, "Using external face tracker: {}", context.faceTracker->name());
    } else if (m_impl->tracker->initialize()) {
        m_graph.setStageEnabled(EffectStage::FaceTracking, true);
    } else {
        m_graph.setStageEnabled(EffectStage::FaceTracking, false);
    }

#ifdef _WIN32
    m_impl->compositor = std::make_unique<Compositor>();
    if (!m_impl->compositor->initialize(context.device, context.context)) {
        HAOCAM_LOG_ERROR(kCategory, "GPU compositor failed to initialize; preview disabled");
        m_impl->compositor.reset();
    }
#else
    (void)context;
    HAOCAM_LOG_INFO(kCategory, "Non-Windows build: GPU compositor not available");
#endif

    // Phase 2+: register providers here. Registration must never be fatal:
    //   registerProvider(makeFacebetterProvider());  // Phase 2
    //   registerProvider(makeOpenMakeupProvider());  // Phase 3
    //   registerProvider(makeSnapCameraProvider());  // Phase 4
    HAOCAM_LOG_INFO(kCategory,
                    "Effect pipeline initialized (beauty/makeup/AR providers arrive in "
                    "Phases 2-4)");

    m_impl->initialized = true;
    return true;
}

void EffectManager::shutdown() {
    if (!m_impl->initialized) return;
#ifdef _WIN32
    if (m_impl->compositor) {
        m_impl->compositor->shutdown();
        m_impl->compositor.reset();
    }
#endif
    if (m_impl->tracker) {
        m_impl->tracker->shutdown();
        m_impl->tracker.reset();
    }
    m_impl->initialized = false;
    HAOCAM_LOG_INFO(kCategory, "Effect pipeline shut down");
}

IBeautyProvider* EffectManager::beauty() {
    return nullptr; // Phase 2
}

IMakeupProvider* EffectManager::makeup() {
    return nullptr; // Phase 3
}

IARProvider* EffectManager::ar() {
    return nullptr; // Phase 4
}

void EffectManager::process(Frame& frame) {
    if (!frame.isValid()) return;

#ifdef _WIN32
    // Stage 2: color conversion (+ color adjustments) and stage 9: final
    // composite run in the GPU compositor. Tracking/beauty/makeup/AR stages
    // are inserted here as they come online in later phases.
    if (m_impl->compositor) {
        m_impl->compositor->process(frame);
    }
#else
    (void)frame;
#endif
}

void EffectManager::reset() {
    // Phase 2+: forward reset to all providers.
    HAOCAM_LOG_DEBUG(kCategory, "Effect pipeline reset");
}

Compositor* EffectManager::compositor() const {
#ifdef _WIN32
    return m_impl->compositor.get();
#else
    return nullptr;
#endif
}

std::vector<EffectManager::ProviderStatus> EffectManager::providerStatus() const {
    std::vector<ProviderStatus> status;
    status.push_back({"Beauty", "Facebetter", false,
                      "Unavailable: SDK not integrated yet (Phase 2). Camera preview "
                      "is unaffected."});
    status.push_back({"Makeup", "OpenMakeupSDK", false,
                      "Unavailable: SDK not integrated yet (Phase 3, web runtime)."});
    status.push_back({"AR", "Snap Camera Kit", false,
                      "Unavailable: SDK not integrated yet (Phase 4, web runtime)."});
    status.push_back({"Native", "HaoCam GPU passes", true, "Active: color conversion + composite."});
    return status;
}

} // namespace haocam
