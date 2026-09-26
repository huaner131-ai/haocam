#pragma once

// Semantic landmark access (Phase 2, spec section 10).
//
// Application logic must NOT hardcode tracker-specific indices. Use the
// FaceLandmark enum + getLandmark(); the mapping is resolved per
// FaceData::landmarkLayout inside this header.
//
// Supported layouts:
//   Canonical68   - classic multi_tracker landmarks
//   MediaPipe468  - MediaPipe FaceMesh dense layout (indices maintained by
//                   the MediaPipe adapter only)
//
// Head pose conventions are documented in FaceData.h (HeadPose).

#include <cstddef>

#include "face/FaceData.h"

namespace haocam::face {

enum class FaceLandmark {
    NoseTip,
    Chin,
    LeftEyeCenter,    // subject's left eye
    RightEyeCenter,   // subject's right eye
    LeftEyeOuter,
    RightEyeOuter,
    LeftMouthCorner,
    RightMouthCorner,
    UpperLip,
    LowerLip,
    LeftCheek,
    RightCheek,
    Forehead,
};

// Returns the semantic landmark in normalized coordinates. When the layout
// does not carry the requested point (or no face is detected), returns
// {NaN, NaN} - callers should check with hasLandmark().
Point2D getLandmark(const FaceData& face, FaceLandmark landmark);

bool hasLandmark(const FaceData& face, FaceLandmark landmark);

// ---------------------------------------------------------------------------
// Canonical 68-point indices (jaw 0-16, brows 17-26, nose 27-35, eyes 36-47,
// mouth 48-67).
// ---------------------------------------------------------------------------
constexpr size_t k68NoseTip = 30;
constexpr size_t k68Chin = 8;
constexpr size_t k68LeftEyeCenter = 39;  // average of 36..41 in getLandmark
constexpr size_t k68RightEyeCenter = 45; // average of 42..47 in getLandmark
constexpr size_t k68LeftEyeOuter = 36;
constexpr size_t k68RightEyeOuter = 45;
constexpr size_t k68LeftMouthCorner = 48;
constexpr size_t k68RightMouthCorner = 54;
constexpr size_t k68UpperLip = 51;
constexpr size_t k68LowerLip = 57;
constexpr size_t k68LeftCheek = 1;   // jaw start (approximation)
constexpr size_t k68RightCheek = 15; // jaw end (approximation)
constexpr size_t k68Forehead = 0;    // jaw has no forehead; top of jaw used

// MediaPipe FaceMesh indices used by the semantic mapping (MediaPipe468).
// These constants live ONLY here - never use them elsewhere in HaoCam.
constexpr size_t kMpNoseTip = 1;
constexpr size_t kMpChin = 152;
constexpr size_t kMpLeftEyeOuter = 33;
constexpr size_t kMpLeftEyeInner = 133;
constexpr size_t kMpLeftEyeTop = 159;
constexpr size_t kMpLeftEyeBottom = 145;
constexpr size_t kMpRightEyeInner = 362;
constexpr size_t kMpRightEyeOuter = 263;
constexpr size_t kMpRightEyeTop = 386;
constexpr size_t kMpRightEyeBottom = 374;
constexpr size_t kMpLeftMouthCorner = 61;
constexpr size_t kMpRightMouthCorner = 291;
constexpr size_t kMpUpperLip = 13;
constexpr size_t kMpLowerLip = 14;
constexpr size_t kMpLeftCheek = 234;
constexpr size_t kMpRightCheek = 454;
constexpr size_t kMpForehead = 10;

// Estimated head pose from semantic landmarks (yaw/pitch/roll in degrees,
// conventions per HeadPose). Lightweight geometric approximation - the
// MediaPipe adapter overrides it with the transformation matrix when the
// model provides one.
HeadPose estimateHeadPose(const FaceData& face);

} // namespace haocam::face
