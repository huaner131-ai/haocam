#pragma once

// Shared face data model (architecture spec, section 13; Phase 2 refinement).
//
// A single face tracking pass produces this structure once per frame; every
// effect provider (beauty / makeup / AR) consumes the same normalized data
// instead of running its own detector.
//
// Landmark coordinates are NORMALIZED to the frame:
//   X = 0.0 (left edge)  -> 1.0 (right edge)
//   Y = 0.0 (top edge)   -> 1.0 (bottom edge)
// Tracker-specific structures (e.g. MediaPipe types) never leak past the
// tracker adapter - only this header is consumed by the rest of HaoCam.

#include <cstdint>
#include <vector>

namespace haocam {

struct Point2D {
    float x = 0.0f;
    float y = 0.0f;
};

// Head pose conventions (documented, never silently inverted):
//   yaw   : left/right rotation.   positive = face turned to THEIR left
//           (viewer's right), i.e. negative Z rotation of the head.
//   pitch : up/down rotation.      positive = looking up.
//   roll  : head tilt.             positive = top of head tilted toward
//           viewer's right.
// All values are degrees in [-180, 180].
struct HeadPose {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
};

// Normalized face bounding box (same coordinate space as landmarks).
struct FaceBounds {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float centerX() const { return x + width * 0.5f; }
    float centerY() const { return y + height * 0.5f; }
};

// Identifies which landmark layout `FaceData::landmarks` uses so semantic
// accessors (getLandmark) can map names to indices per layout.
enum class LandmarkLayout : uint8_t {
    None = 0,        // no landmarks
    Canonical68,     // classic 68-point layout
    MediaPipe468,    // MediaPipe FaceMesh 468-point layout (+ optional iris)
};

// Rigid face transform applied by the reshape stage (set by providers).
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
    LandmarkLayout landmarkLayout = LandmarkLayout::None;

    HeadPose headPose;
    FaceBounds bounds;

    // Blend shape coefficients (AR providers), typically 52 ARKit-style slots.
    std::vector<float> blendShapes;

    FaceTransform transform;

    void reset() { *this = FaceData(); }

    // Computes the tight bounding box of the current landmarks (normalized
    // space). No-op when there are no landmarks.
    void updateBoundsFromLandmarks();
};

} // namespace haocam
