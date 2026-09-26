#pragma once

// Canonical landmark topology constants used across providers.
//
// HaoCam normalizes every tracker to a superset layout:
//   * 0..67   : the classic 68-point layout (jaw, brows, nose, eyes, mouth)
//   * 68..    : extended dense landmarks when a tracker provides them
//               (e.g. MediaPipe FaceMesh 468 points mapped 1:1)
//
// Providers must not assume a specific tracker is active; they read
// FaceData.landmarks.size() and degrade gracefully.

#include <cstddef>

namespace haocam::face {

// 68-point canonical indices.
constexpr size_t kJawStart = 0;
constexpr size_t kJawCount = 17;
constexpr size_t kLeftEyebrowStart = 17;
constexpr size_t kRightEyebrowStart = 22;
constexpr size_t kNoseStart = 27;
constexpr size_t kLeftEyeStart = 36;
constexpr size_t kRightEyeStart = 42;
constexpr size_t kMouthStart = 48;
constexpr size_t kCanonical68 = 68;

// MediaPipe FaceMesh subsets frequently needed by beauty/makeup passes.
constexpr size_t kFaceMeshPointCount = 468;
constexpr size_t kLeftEyeIrisCenter = 468; // with refine_landmarks=true
constexpr size_t kRightEyeIrisCenter = 473;
constexpr size_t kLipsOuterStart = 61;

inline bool hasCanonical68(size_t landmarkCount) {
    return landmarkCount >= kCanonical68;
}

} // namespace haocam::face
