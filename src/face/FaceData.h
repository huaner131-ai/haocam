#pragma once

// Shared face data model (architecture spec, section 13).
//
// A single face tracking pass produces this structure once per frame; every
// effect provider (beauty / makeup / AR) consumes the same normalized data
// instead of running its own detector.

#include <cstdint>
#include <vector>

namespace haocam {

struct Point2D {
    float x = 0.0f;
    float y = 0.0f;
};

struct HeadPose {
    float yaw = 0.0f;   // degrees
    float pitch = 0.0f;
    float roll = 0.0f;
};

struct FaceBounds {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Rigid face transform applied by the reshape stage (Phase 2+).
struct FaceTransform {
    float scale = 1.0f;
    float translateX = 0.0f;
    float translateY = 0.0f;
    float rotation = 0.0f;
};

struct FaceData {
    bool detected = false;
    float confidence = 0.0f;

    // Normalized landmark coordinates in [0,1] frame space.
    std::vector<Point2D> landmarks;

    HeadPose headPose;
    FaceBounds bounds;

    // Blend shape coefficients (AR providers), typically 52 ARKit-style slots.
    std::vector<float> blendShapes;

    FaceTransform transform;

    void reset() { *this = FaceData(); }
};

} // namespace haocam
