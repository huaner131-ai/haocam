#pragma once

// Face tracker interface (Phase 2, spec sections 6/7/8).
//
// Trackers are providers exactly like beauty/makeup/AR engines: the rest of
// HaoCam depends only on this interface and on FaceData/FaceTrackingResult -
// never on MediaPipe (or any other tracker SDK) directly.
//
//   IFaceTracker
//     |-- NullFaceTracker        (always compiled; honest "unavailable")
//     |-- MediaPipeFaceTracker   (compiled when HAOCAM_ENABLE_MEDIAPIPE
//                                 and the SDK drop-in exists)
//     +-- (future: FacebetterTracker / NativeTracker)
//
// Threading: initialize()/shutdown() run on the tracking worker thread;
// process() is called only from that thread. Other threads may read the
// published FaceTrackingResult snapshot (owned by TrackingWorker).

#include <memory>
#include <string>
#include <vector>

#include "face/FaceData.h"
#include "frame/Frame.h"

namespace haocam {

enum class TrackerBackend {
    None,       // no tracker available
    MediaPipe,  // MediaPipe Tasks FaceLandmarker
    Provider,   // tracker embedded in an SDK provider (future)
};

const char* toStringTrackerBackend(TrackerBackend backend);

struct FaceTrackerConfig {
    // Path to the model asset bundle (MediaPipe: face_landmarker.task).
    // Never downloaded at runtime - see assets/models/README.md.
    std::string modelAssetPath;

    float minDetectionConfidence = 0.5f;
    float minPresenceConfidence = 0.5f;
    float minTrackingConfidence = 0.5f;

    // Primary face first; additional faces best-effort (Phase 2 uses 1).
    int maxFaces = 1;

    // Output toggles (when supported by the backend).
    bool outputBlendShapes = true;
    bool outputHeadPoseMatrix = true;

    // Temporal smoothing (LandmarkSmoother).
    float smoothingMinCutoff = 1.2f;
    float smoothingBeta = 0.007f;

    // Upper bound on tracker invocations (stale frames are dropped instead
    // of queued - spec section 13).
    float maxTrackingFps = 30.0f;
};

struct FaceTrackingResult {
    bool detected = false;
    float confidence = 0.0f;

    // Primary face is faces[0]. Phase 2 populates exactly one; the vector
    // exists so multi-face support needs no API change.
    std::vector<FaceData> faces;

    uint64_t timestamp = 0; // capture timestamp of the tracked frame (us)

    bool empty() const { return !detected || faces.empty(); }

    // Convenience: primary face or nullptr.
    const FaceData* primary() const {
        return detected && !faces.empty() ? &faces.front() : nullptr;
    }
};

// Pixel view handed to trackers that consume raw RGBA (the tracking worker
// owns the buffer; trackers must not retain the pointer).
struct TrackingFrameView {
    const uint8_t* rgba = nullptr; // tightly packed, w*h*4 bytes
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t strideBytes = 0; // == width * 4
    uint64_t timestampUs = 0;
};

// Optional extension for pixel-consuming trackers. IFaceTracker::process()
// stays the spec API (frame-level); trackers needing actual pixels override
// processRGBA and the tracking worker routes through it.
class IPixelSink {
public:
    virtual ~IPixelSink() = default;
    virtual FaceTrackingResult processRGBA(const TrackingFrameView& view) = 0;
};

class IFaceTracker {
public:
    virtual ~IFaceTracker() = default;

    // Loads models / allocates working buffers. Returns false and reports
    // the reason via statusText() when the backend cannot run (missing
    // model, missing SDK, license, ...). Never fatal to the host app.
    virtual bool initialize(const FaceTrackerConfig& config) = 0;

    virtual void shutdown() = 0;

    virtual bool isAvailable() const = 0;

    // Human-readable status for Settings/diagnostics, e.g.
    // "Ready", "Unavailable: model file not found at <path>".
    virtual std::string statusText() const = 0;

    virtual const char* name() const = 0;
    virtual TrackerBackend backend() const = 0;

    // Tracks faces in the input frame. Implementations must be cheap to
    // call with an invalid/empty frame (returns an undetected result).
    virtual FaceTrackingResult process(const Frame& frame) = 0;
};

// Null tracker: always reports "not detected", never fails the pipeline.
class NullFaceTracker final : public IFaceTracker {
public:
    bool initialize(const FaceTrackerConfig& config) override;
    void shutdown() override;
    bool isAvailable() const override { return false; }
    std::string statusText() const override { return m_status; }
    const char* name() const override { return "NullTracker"; }
    TrackerBackend backend() const override { return TrackerBackend::None; }
    FaceTrackingResult process(const Frame& frame) override;

private:
    std::string m_status = "Unavailable: no tracker backend compiled";
};

// Returns the best available tracker for this build. Currently MediaPipe
// when compiled in, otherwise the null tracker (honest unavailability).
std::unique_ptr<IFaceTracker> createDefaultTracker();

} // namespace haocam
