/**
 * HuanFace Tracking State — Phase 5 Production
 * Temporal tracking, ID persistence, smoothing EMA
 */

#pragma once
#include "face_data.h"
#include "face_detector.h"
#include "../../include/huanface_c_api.h"
#include <vector>
#include <map>

namespace huanface {

struct TrackedFace {
    int id;
    FaceDetection lastDetection;
    HFFaceData lastFaceData;
    int64_t lastSeenTimestamp;
    int lostFrames;
    int totalFramesTracked;
    float smoothedConfidence;

    // For EMA smoothing
    std::vector<HFVec2> smoothedLandmarks;
    HFFacePose smoothedPose;
};

class TemporalTracker {
public:
    TemporalTracker() = default;
    ~TemporalTracker() = default;

    HFResult Init(const HFEngineConfigC& config);
    void Shutdown();
    HFResult Update(const std::vector<FaceDetection>& detections,
                    const std::vector<HFFaceData>& faceDatas,
                    int64_t timestampNanos,
                    std::vector<HFFaceData>& outTrackedFaces);

    void SetSmoothingAlpha(float alpha) { smoothingAlpha = alpha; }
    void SetMaxLostFrames(int max) { maxLostFrames = max; }
    void SetIoUThreshold(float thresh) { iouThreshold = thresh; }

    std::string GetName() const { return "TemporalTracker"; }

private:
    bool initialized = false;
    float smoothingAlpha = 0.6f; // EMA alpha: higher = more responsive, lower = smoother
    int maxLostFrames = 10; // frames to keep lost face before removing
    float iouThreshold = 0.3f; // IoU threshold for matching
    int nextId = 0;

    std::map<int, TrackedFace> trackedFaces; // id -> tracked face

    int FindMatchingFace(const FaceDetection& detection);
    void SmoothLandmarks(const std::vector<HFVec2>& current, std::vector<HFVec2>& smoothed);
    void SmoothPose(const HFFacePose& current, HFFacePose& smoothed);
};

} // namespace huanface
