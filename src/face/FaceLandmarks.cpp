#include "face/FaceLandmarks.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace haocam {

void FaceData::updateBoundsFromLandmarks() {
    if (landmarks.empty()) {
        bounds = FaceBounds{};
        return;
    }
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    for (const Point2D& p : landmarks) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    bounds.x = minX;
    bounds.y = minY;
    bounds.width = maxX - minX;
    bounds.height = maxY - minY;
}

namespace face {

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

Point2D nanPoint() { return Point2D{kNaN, kNaN}; }

Point2D averageOf(const FaceData& face, std::initializer_list<size_t> indices) {
    float x = 0.0f;
    float y = 0.0f;
    int used = 0;
    for (size_t index : indices) {
        if (index < face.landmarks.size()) {
            x += face.landmarks[index].x;
            y += face.landmarks[index].y;
            ++used;
        }
    }
    if (used == 0) return nanPoint();
    return Point2D{x / used, y / used};
}

Point2D atOr(const FaceData& face, size_t index) {
    return index < face.landmarks.size() ? face.landmarks[index] : nanPoint();
}

} // namespace

Point2D getLandmark(const FaceData& face, FaceLandmark landmark) {
    if (!face.detected || face.landmarks.empty()) return nanPoint();

    switch (face.landmarkLayout) {
        case LandmarkLayout::Canonical68: {
            switch (landmark) {
                case FaceLandmark::NoseTip: return atOr(face, k68NoseTip);
                case FaceLandmark::Chin: return atOr(face, k68Chin);
                case FaceLandmark::LeftEyeCenter:
                    return averageOf(face, {36, 37, 38, 39, 40, 41});
                case FaceLandmark::RightEyeCenter:
                    return averageOf(face, {42, 43, 44, 45, 46, 47});
                case FaceLandmark::LeftEyeOuter: return atOr(face, k68LeftEyeOuter);
                case FaceLandmark::RightEyeOuter: return atOr(face, k68RightEyeOuter);
                case FaceLandmark::LeftMouthCorner: return atOr(face, k68LeftMouthCorner);
                case FaceLandmark::RightMouthCorner: return atOr(face, k68RightMouthCorner);
                case FaceLandmark::UpperLip: return atOr(face, k68UpperLip);
                case FaceLandmark::LowerLip: return atOr(face, k68LowerLip);
                case FaceLandmark::LeftCheek: return atOr(face, k68LeftCheek);
                case FaceLandmark::RightCheek: return atOr(face, k68RightCheek);
                case FaceLandmark::Forehead: return atOr(face, k68Forehead);
            }
            break;
        }
        case LandmarkLayout::MediaPipe468: {
            switch (landmark) {
                case FaceLandmark::NoseTip: return atOr(face, kMpNoseTip);
                case FaceLandmark::Chin: return atOr(face, kMpChin);
                case FaceLandmark::LeftEyeCenter:
                    return averageOf(face, {kMpLeftEyeOuter, kMpLeftEyeInner,
                                            kMpLeftEyeTop, kMpLeftEyeBottom});
                case FaceLandmark::RightEyeCenter:
                    return averageOf(face, {kMpRightEyeInner, kMpRightEyeOuter,
                                            kMpRightEyeTop, kMpRightEyeBottom});
                case FaceLandmark::LeftEyeOuter: return atOr(face, kMpLeftEyeOuter);
                case FaceLandmark::RightEyeOuter: return atOr(face, kMpRightEyeOuter);
                case FaceLandmark::LeftMouthCorner: return atOr(face, kMpLeftMouthCorner);
                case FaceLandmark::RightMouthCorner: return atOr(face, kMpRightMouthCorner);
                case FaceLandmark::UpperLip: return atOr(face, kMpUpperLip);
                case FaceLandmark::LowerLip: return atOr(face, kMpLowerLip);
                case FaceLandmark::LeftCheek: return atOr(face, kMpLeftCheek);
                case FaceLandmark::RightCheek: return atOr(face, kMpRightCheek);
                case FaceLandmark::Forehead: return atOr(face, kMpForehead);
            }
            break;
        }
        case LandmarkLayout::None:
            break;
    }
    return nanPoint();
}

bool hasLandmark(const FaceData& face, FaceLandmark landmark) {
    const Point2D p = getLandmark(face, landmark);
    return !std::isnan(p.x) && !std::isnan(p.y);
}

HeadPose estimateHeadPose(const FaceData& face) {
    HeadPose pose;
    if (!face.detected) return pose;

    const Point2D leftEye = getLandmark(face, FaceLandmark::LeftEyeCenter);
    const Point2D rightEye = getLandmark(face, FaceLandmark::RightEyeCenter);
    const Point2D noseTip = getLandmark(face, FaceLandmark::NoseTip);
    const Point2D chin = getLandmark(face, FaceLandmark::Chin);
    const Point2D forehead = getLandmark(face, FaceLandmark::Forehead);

    // Roll from the eye line.
    if (!std::isnan(leftEye.x) && !std::isnan(rightEye.x)) {
        pose.roll = std::atan2(rightEye.y - leftEye.y, rightEye.x - leftEye.x) *
                    180.0f / 3.14159265358979f;
    }

    // Yaw approximation: nose tip horizontal offset from the eye-line middle,
    // normalized by face width.
    if (!std::isnan(noseTip.x) && !std::isnan(leftEye.x) && !std::isnan(rightEye.x)) {
        const float eyeMiddleX = (leftEye.x + rightEye.x) * 0.5f;
        const float eyeDistance = std::max(1e-4f, std::abs(rightEye.x - leftEye.x));
        const float offset = (noseTip.x - eyeMiddleX) / eyeDistance;
        // ~±45 degrees mapped over a typical offset range; clamped, monotonic.
        pose.yaw = std::clamp(offset * 90.0f, -180.0f, 180.0f);
    }

    // Pitch approximation: nose tip vertical position between forehead and
    // chin (0.5 = neutral).
    if (!std::isnan(noseTip.y) && !std::isnan(forehead.y) && !std::isnan(chin.y)) {
        const float span = chin.y - forehead.y;
        if (std::abs(span) > 1e-4f) {
            const float ratio = (noseTip.y - forehead.y) / span; // ~0.45 neutral
            pose.pitch = std::clamp((ratio - 0.45f) * -180.0f, -180.0f, 180.0f);
        }
    }

    return pose;
}

} // namespace face
} // namespace haocam
