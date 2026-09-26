#pragma once

// MediaPipe Tasks FaceLandmarker adapter (Phase 2, spec sections 6/12/38).
//
// The rest of HaoCam depends only on IFaceTracker - never on MediaPipe.
// This adapter:
//   * consumes tightly-packed RGBA8 pixels (TrackingFrameView) produced by
//     the tracking worker's GPU downscale/readback (Windows) or the CPU
//     NV12 conversion (fallback);
//   * wraps the official MediaPipe Tasks C++ API
//     (mediapipe::tasks::vision::face_landmarker::FaceLandmarker,
//     VIDEO running mode, DetectForVideo with monotonic timestamps);
//   * maps MediaPipe 468-point landmarks into HaoCam's normalized FaceData
//     (LandmarkLayout::MediaPipe468) - MediaPipe types never leak;
//   * derives head pose from the facial transformation matrix when enabled;
//   * smooths landmarks (TrackingWorker owns the LandmarkSmoother).
//
// COMPILED ONLY when HAOCAM_ENABLE_MEDIAPIPE=ON and the SDK drop-in exists
// under sdk/mediapipe/ (see docs/FACE_TRACKING.md). The model bundle
// (face_landmarker.task) is NOT committed and never downloaded at runtime;
// a missing model => tracker reports Unavailable.

#include <memory>

#include "face/FaceTracker.h"

namespace haocam {

class MediaPipeFaceTracker final : public IFaceTracker, public IPixelSink {
public:
    MediaPipeFaceTracker();
    ~MediaPipeFaceTracker() override;

    MediaPipeFaceTracker(const MediaPipeFaceTracker&) = delete;
    MediaPipeFaceTracker& operator=(const MediaPipeFaceTracker&) = delete;

    // True when the MediaPipe runtime (shared libraries) is loadable in this
    // process. Checked before initialize(); false => honest unavailability.
    bool backendAvailable() const;

    // IFaceTracker
    bool initialize(const FaceTrackerConfig& config) override;
    void shutdown() override;
    bool isAvailable() const override;
    std::string statusText() const override;
    const char* name() const override { return "MediaPipeFaceTracker"; }
    TrackerBackend backend() const override { return TrackerBackend::MediaPipe; }
    FaceTrackingResult process(const Frame& frame) override;

    // IPixelSink (the tracking worker routes here)
    FaceTrackingResult processRGBA(const TrackingFrameView& view) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace haocam
