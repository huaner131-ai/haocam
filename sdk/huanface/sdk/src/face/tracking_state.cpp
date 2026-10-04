/**
 * Temporal Tracker Implementation — Phase 5
 * ID persistence via IoU, EMA smoothing
 */

#include "tracking_state.h"
#include <cmath>
#include <algorithm>

namespace huanface {

HFResult TemporalTracker::Init(const HFEngineConfigC& config) {
    (void)config;
    smoothingAlpha = 0.6f;
    maxLostFrames = 10;
    iouThreshold = 0.3f;
    nextId = 0;
    trackedFaces.clear();
    initialized = true;
    return HF_RESULT_OK;
}

void TemporalTracker::Shutdown() {
    trackedFaces.clear();
    initialized = false;
}

int TemporalTracker::FindMatchingFace(const FaceDetection& detection) {
    int bestId = -1;
    float bestIoU = iouThreshold;
    for (auto& kv : trackedFaces) {
        float iou = detection.IoU(kv.second.lastDetection);
        if (iou > bestIoU) {
            bestIoU = iou;
            bestId = kv.first;
        }
    }
    return bestId;
}

void TemporalTracker::SmoothLandmarks(const std::vector<HFVec2>& current, std::vector<HFVec2>& smoothed) {
    if (smoothed.empty()) {
        smoothed = current;
        return;
    }
    if (smoothed.size() != current.size()) {
        smoothed = current;
        return;
    }
    for (size_t i=0;i<current.size();++i) {
        smoothed[i].x = smoothingAlpha * current[i].x + (1.0f - smoothingAlpha) * smoothed[i].x;
        smoothed[i].y = smoothingAlpha * current[i].y + (1.0f - smoothingAlpha) * smoothed[i].y;
    }
}

void TemporalTracker::SmoothPose(const HFFacePose& current, HFFacePose& smoothed) {
    // If first time, copy
    if (smoothed.scale == 0) {
        smoothed = current;
        return;
    }
    smoothed.yaw = smoothingAlpha * current.yaw + (1.0f - smoothingAlpha) * smoothed.yaw;
    smoothed.pitch = smoothingAlpha * current.pitch + (1.0f - smoothingAlpha) * smoothed.pitch;
    smoothed.roll = smoothingAlpha * current.roll + (1.0f - smoothingAlpha) * smoothed.roll;
    smoothed.tx = smoothingAlpha * current.tx + (1.0f - smoothingAlpha) * smoothed.tx;
    smoothed.ty = smoothingAlpha * current.ty + (1.0f - smoothingAlpha) * smoothed.ty;
    smoothed.tz = smoothingAlpha * current.tz + (1.0f - smoothingAlpha) * smoothed.tz;
    smoothed.scale = smoothingAlpha * current.scale + (1.0f - smoothingAlpha) * smoothed.scale;
}

HFResult TemporalTracker::Update(const std::vector<FaceDetection>& detections,
                                  const std::vector<HFFaceData>& faceDatas,
                                  int64_t timestampNanos,
                                  std::vector<HFFaceData>& outTrackedFaces) {
    if (!initialized) return HF_RESULT_NOT_INITIALIZED;
    if (detections.size() != faceDatas.size()) return HF_RESULT_INVALID_PARAM;

    outTrackedFaces.clear();
    outTrackedFaces.reserve(detections.size());

    std::map<int, bool> seenIds; // id -> seen this frame

    for (size_t i=0;i<detections.size();++i) {
        const FaceDetection& det = detections[i];
        const HFFaceData& data = faceDatas[i];

        int matchedId = FindMatchingFace(det);
        HFFaceData trackedData = data;

        if (matchedId >= 0) {
            // Existing face
            auto it = trackedFaces.find(matchedId);
            if (it != trackedFaces.end()) {
                TrackedFace& tf = it->second;
                // Smooth landmarks
                SmoothLandmarks(data.landmarks, tf.smoothedLandmarks);
                trackedData.landmarks = tf.smoothedLandmarks;

                // Smooth 3D landmarks similarly (x,y smoothed, z kept)
                if (tf.lastFaceData.landmarks3D.size() == data.landmarks3D.size()) {
                    for (size_t j=0;j<data.landmarks3D.size();++j) {
                        tf.lastFaceData.landmarks3D[j].x = smoothingAlpha * data.landmarks3D[j].x + (1.0f - smoothingAlpha) * tf.lastFaceData.landmarks3D[j].x;
                        tf.lastFaceData.landmarks3D[j].y = smoothingAlpha * data.landmarks3D[j].y + (1.0f - smoothingAlpha) * tf.lastFaceData.landmarks3D[j].y;
                        tf.lastFaceData.landmarks3D[j].z = smoothingAlpha * data.landmarks3D[j].z + (1.0f - smoothingAlpha) * tf.lastFaceData.landmarks3D[j].z;
                    }
                    trackedData.landmarks3D = tf.lastFaceData.landmarks3D;
                }

                // Smooth pose
                SmoothPose(data.pose, tf.smoothedPose);
                trackedData.pose = tf.smoothedPose;

                // Update legacy rotation fields from smoothed pose
                trackedData.rotationYaw = tf.smoothedPose.yaw;
                trackedData.rotationPitch = tf.smoothedPose.pitch;
                trackedData.rotationRoll = tf.smoothedPose.roll;
                trackedData.translationX = tf.smoothedPose.tx;
                trackedData.translationY = tf.smoothedPose.ty;
                trackedData.translationZ = tf.smoothedPose.tz;
                trackedData.scale = tf.smoothedPose.scale;

                // Confidence smoothing
                float smoothedConf = smoothingAlpha * data.detectionConfidence + (1.0f - smoothingAlpha) * tf.smoothedConfidence;
                trackedData.detectionConfidence = smoothedConf;
                trackedData.confidence = smoothedConf;
                tf.smoothedConfidence = smoothedConf;

                // Update tracking state
                if (tf.lostFrames > 0) {
                    trackedData.trackingState = HFTrackingState::REAPPEARED;
                } else {
                    trackedData.trackingState = HFTrackingState::TRACKED;
                }

                trackedData.id = matchedId;
                trackedData.lastSeenTimestamp = timestampNanos;
                trackedData.lostFrames = 0;

                // Update stored
                tf.lastDetection = det;
                tf.lastFaceData = trackedData;
                tf.lastSeenTimestamp = timestampNanos;
                tf.lostFrames = 0;
                tf.totalFramesTracked++;

                seenIds[matchedId] = true;
            } else {
                // Should not happen, treat as new
                matchedId = -1;
            }
        }

        if (matchedId < 0) {
            // New face
            int newId = nextId++;
            trackedData.id = newId;
            trackedData.trackingState = HFTrackingState::DETECTED;
            trackedData.lastSeenTimestamp = timestampNanos;
            trackedData.lostFrames = 0;

            TrackedFace tf;
            tf.id = newId;
            tf.lastDetection = det;
            tf.lastFaceData = trackedData;
            tf.lastSeenTimestamp = timestampNanos;
            tf.lostFrames = 0;
            tf.totalFramesTracked = 1;
            tf.smoothedConfidence = trackedData.detectionConfidence;
            tf.smoothedLandmarks = trackedData.landmarks;
            tf.smoothedPose = trackedData.pose;

            trackedFaces[newId] = tf;
            seenIds[newId] = true;
        }

        outTrackedFaces.push_back(trackedData);
    }

    // Handle lost faces: increment lostFrames, remove if exceeds max
    std::vector<int> toRemove;
    for (auto& kv : trackedFaces) {
        int id = kv.first;
        if (seenIds.find(id) == seenIds.end()) {
            kv.second.lostFrames++;
            if (kv.second.lostFrames > maxLostFrames) {
                toRemove.push_back(id);
            }
        }
    }
    for (int id : toRemove) {
        trackedFaces.erase(id);
    }

    return HF_RESULT_OK;
}

} // namespace huanface
