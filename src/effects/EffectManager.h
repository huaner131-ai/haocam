#pragma once

// The single entry point between the UI and effect providers
// (architecture spec, section 16; Phase 2, sections 23/24).
//
// The UI communicates ONLY with EffectManager - never with Facebetter,
// MediaPipe, OpenMakeup or Snap directly.
//
// Owns:
//   IFaceTracker + TrackingWorker (tracking thread, drop-stale latest slot)
//   IBeautyProvider               (Facebetter adapter or NullBeauty)
//   EffectGraph                   (canonical stage order)
//   Compositor                    (GPU passes, Windows builds)
//
// Engine-thread contract: updateTracking()/process() are called from the
// engine thread only; parameter setters are thread-safe (UI thread).

#include <memory>
#include <vector>

#include "core/config/AppConfig.h"
#include "effects/EffectGraph.h"
#include "effects/EffectProvider.h"
#include "face/FaceTracker.h"
#include "face/TrackingWorker.h"

namespace haocam {

class IBeautyProvider;
class IMakeupProvider;
class IARProvider;
struct EffectContext;
class Compositor;

class EffectManager {
public:
    EffectManager();
    ~EffectManager();

    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

    // Configuration (call before initialize; thread-neutral).
    void setBeautySettings(const core::FacebetterSettings& settings);
    void setTrackingSettings(const core::TrackingSettings& settings);

    // Creates the tracking worker and the beauty provider. Missing SDKs are
    // logged and reported as Unavailable - never fatal (spec section 21).
    bool initialize(const EffectContext& context);
    void shutdown();

    // Provider accessors (nullptr / null-provider semantics preserved).
    IBeautyProvider* beauty();
    IFaceTracker* faceTracker();
    TrackingWorker* trackingWorker() { return m_tracking.get(); }
    Compositor* compositor() const; // Windows/D3D11 builds; nullptr elsewhere

    // Engine thread: submits the processed frame to the tracking worker
    // (latest-slot; tracking runs off the UI thread per spec section 13).
    void updateTracking(const Frame& frame);

    // Engine thread: advances the pipeline for one frame.
    //   colorConvert -> [beauty override if fresh] -> composite -> publish
    //   then feeds tracking + beauty with the processed texture.
    void process(Frame& frame);

    void reset();

    EffectGraph& graph() { return m_graph; }
    const EffectGraph& graph() const { return m_graph; }

    // Status registry for the Settings panel (one entry per provider slot).
    struct ProviderStatus {
        std::string slot;
        std::string provider;
        bool available = false;
        std::string detail;
    };
    std::vector<ProviderStatus> providerStatus() const;

    // Diagnostics (spec section 34).
    struct BeautyDiagnostics {
        bool enabled = false;
        bool available = false;
        double readbackMs = 0.0;
        double sdkProcessMs = 0.0;
        double uploadMs = 0.0;
        int detectedFaces = 0;
        uint64_t processedFrames = 0;
    };
    BeautyDiagnostics beautyDiagnostics() const;

    FaceTrackingResult latestTracking() const;

private:
    FaceTrackerConfig trackingConfig() const;

    std::unique_ptr<TrackingWorker> m_tracking;
    std::unique_ptr<IBeautyProvider> m_beauty; // NullBeauty default; Facebetter
                                               // adapter compiled in when enabled
#ifdef _WIN32
    std::unique_ptr<Compositor> m_compositor;
#endif

    core::FacebetterSettings m_beautySettings;
    core::TrackingSettings m_trackingSettings;
    EffectGraph m_graph;
    FaceTrackingResult m_latestTracking; // engine thread only
    uint64_t m_lastBeautyFrameId = 0;
    bool m_initialized = false;
};

} // namespace haocam
