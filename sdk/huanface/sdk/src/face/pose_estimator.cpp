/**
 * Production Pose Estimator Implementation — Phase 5
 * Real pose from landmarks geometry
 */

#include "pose_estimator.h"
#include <cmath>
#include <algorithm>

namespace huanface {

HFResult ProductionPoseEstimator::Init(const HFEngineConfigC& config) {
    (void)config;
    initialized = true;
    return HF_RESULT_OK;
}

void ProductionPoseEstimator::Shutdown() {
    initialized = false;
}

float ProductionPoseEstimator::ComputeRoll(const std::vector<HFVec2>& landmarks) const {
    if (landmarks.size() < 48) return 0.0f;
    // Roll from eyes: angle between left and right eye centers
    HFVec2 leftEyeCenter(0,0), rightEyeCenter(0,0);
    for (int i=42;i<=47;++i) { leftEyeCenter.x+=landmarks[i].x; leftEyeCenter.y+=landmarks[i].y; }
    leftEyeCenter.x/=6; leftEyeCenter.y/=6;
    for (int i=36;i<=41;++i) { rightEyeCenter.x+=landmarks[i].x; rightEyeCenter.y+=landmarks[i].y; }
    rightEyeCenter.x/=6; rightEyeCenter.y/=6;

    float dx = rightEyeCenter.x - leftEyeCenter.x;
    float dy = rightEyeCenter.y - leftEyeCenter.y;
    float angleRad = std::atan2(dy, dx);
    float angleDeg = angleRad * 180.0f / 3.14159f;
    // Roll is angle of eye line, 0 = horizontal
    return angleDeg;
}

float ProductionPoseEstimator::ComputeYaw(const std::vector<HFVec2>& landmarks, const FaceDetection& face) const {
    if (landmarks.size() < 48) return 0.0f;
    // Yaw from nose position relative to eyes and face center
    HFVec2 leftEyeCenter(0,0), rightEyeCenter(0,0);
    for (int i=42;i<=47;++i) { leftEyeCenter.x+=landmarks[i].x; leftEyeCenter.y+=landmarks[i].y; }
    leftEyeCenter.x/=6; leftEyeCenter.y/=6;
    for (int i=36;i<=41;++i) { rightEyeCenter.x+=landmarks[i].x; rightEyeCenter.y+=landmarks[i].y; }
    rightEyeCenter.x/=6; rightEyeCenter.y/=6;

    HFVec2 eyeMid((leftEyeCenter.x+rightEyeCenter.x)*0.5f, (leftEyeCenter.y+rightEyeCenter.y)*0.5f);
    HFVec2 noseTip = landmarks[30]; // nose tip

    // Distance from nose to left eye vs right eye
    float distToLeft = noseTip.x - leftEyeCenter.x;
    float distToRight = rightEyeCenter.x - noseTip.x;
    float total = distToLeft + distToRight;
    if (total <= 0.001f) return 0.0f;

    float ratio = (distToLeft - distToRight) / total; // -1 to 1
    // Yaw: positive = face turns right (left side more visible, nose closer to right eye)
    float yaw = ratio * 60.0f; // scale to degrees, max ~60 deg

    // Also consider face bbox center vs nose
    float faceCenterX = face.x + face.w*0.5f;
    float noseOffset = noseTip.x - faceCenterX;
    float offsetYaw = (noseOffset / (face.w*0.5f)) * 30.0f;
    yaw = yaw*0.7f + offsetYaw*0.3f;

    return std::max(-90.0f, std::min(90.0f, yaw));
}

float ProductionPoseEstimator::ComputePitch(const std::vector<HFVec2>& landmarks, const FaceDetection& face) const {
    if (landmarks.size() < 68) return 0.0f;
    // Pitch from vertical position of nose relative to eyes and mouth
    HFVec2 leftEyeCenter(0,0), rightEyeCenter(0,0);
    for (int i=42;i<=47;++i) { leftEyeCenter.x+=landmarks[i].x; leftEyeCenter.y+=landmarks[i].y; }
    leftEyeCenter.x/=6; leftEyeCenter.y/=6;
    for (int i=36;i<=41;++i) { rightEyeCenter.x+=landmarks[i].x; rightEyeCenter.y+=landmarks[i].y; }
    rightEyeCenter.x/=6; rightEyeCenter.y/=6;

    HFVec2 eyeMid((leftEyeCenter.x+rightEyeCenter.x)*0.5f, (leftEyeCenter.y+rightEyeCenter.y)*0.5f);
    HFVec2 noseTip = landmarks[30];
    HFVec2 mouthCenter(0,0);
    for (int i=48;i<=60;++i) { mouthCenter.x+=landmarks[i].x; mouthCenter.y+=landmarks[i].y; }
    mouthCenter.x/=13; mouthCenter.y/=13;

    float eyeToMouth = mouthCenter.y - eyeMid.y;
    float eyeToNose = noseTip.y - eyeMid.y;

    if (eyeToMouth <= 5.0f) return 0.0f; // too small, treat as frontal

    float ratio = eyeToNose / eyeToMouth; // typical ~0.5-0.6 when frontal
    // Clamp ratio to reasonable frontal range to avoid extreme pitch from noisy detection
    if (ratio < 0.2f || ratio > 0.9f) {
        // Likely noisy detection, fallback to 0 pitch for frontal assumption
        // But still compute small pitch based on clamped ratio
        ratio = std::max(0.3f, std::min(0.75f, ratio));
    }
    // When looking down, nose appears closer to mouth (ratio higher)
    // When looking up, nose closer to eyes (ratio lower)
    float pitch = (ratio - 0.52f) * 80.0f; // reduced scale from 120 to 80 for stability

    return std::max(-90.0f, std::min(90.0f, pitch));
}

HFResult ProductionPoseEstimator::Estimate(const std::vector<HFVec2>& landmarks,
                                            const FaceDetection& face,
                                            int imageWidth, int imageHeight,
                                            HFFacePose& outPose) {
    if (!initialized) return HF_RESULT_NOT_INITIALIZED;
    if (landmarks.size() < 5) return HF_RESULT_INVALID_PARAM;

    float roll = ComputeRoll(landmarks);
    float yaw = ComputeYaw(landmarks, face);
    float pitch = ComputePitch(landmarks, face);

    // Translation: face center
    float tx = face.x + face.w*0.5f;
    float ty = face.y + face.h*0.5f;
    // Tz estimated from face size: larger face = closer (smaller tz)
    float referenceSize = 200.0f; // reference face width at 1m distance
    float scale = face.w / referenceSize;
    float tz = 1000.0f / (scale + 0.1f); // rough depth in mm? Actually arbitrary
    // For simplicity, tz = 500 / scale
    tz = 500.0f / (scale + 0.2f);

    outPose.yaw = yaw;
    outPose.pitch = pitch;
    outPose.roll = roll;
    outPose.tx = tx;
    outPose.ty = ty;
    outPose.tz = tz;
    outPose.scale = scale;

    // Validate no NaN
    if (!std::isfinite(outPose.yaw) || !std::isfinite(outPose.pitch) || !std::isfinite(outPose.roll)) {
        return HF_RESULT_FAIL;
    }

    return HF_RESULT_OK;
}

} // namespace huanface
