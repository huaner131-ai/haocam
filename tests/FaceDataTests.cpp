// Phase 2: FaceData contract (spec sections 9-11).

#include <cmath>
#include <vector>

#include "face/FaceData.h"
#include "face/FaceLandmarks.h"
#include "face/FaceTracker.h"
#include "test_main.h"

using namespace haocam;
using namespace haocam::face;

namespace {
// Canonical68 face with anatomically placed semantic points:
// left eye center (0.45, 0.40), right eye center (0.55, 0.40),
// nose tip (0.50, 0.50), chin (0.50, 0.75), forehead proxy (0.50, 0.30).
FaceData canonicalFace(float noseX = 0.50f, float noseY = 0.50f) {
    FaceData face;
    face.detected = true;
    face.landmarkLayout = LandmarkLayout::Canonical68;
    face.landmarks.assign(68, Point2D{0.5f, 0.5f});
    for (size_t i = 36; i <= 41; ++i) face.landmarks[i] = {0.45f, 0.40f};
    for (size_t i = 42; i <= 47; ++i) face.landmarks[i] = {0.55f, 0.40f};
    face.landmarks[k68NoseTip] = {noseX, noseY};
    face.landmarks[k68Chin] = {0.50f, 0.75f};
    face.landmarks[k68Forehead] = {0.50f, 0.30f};
    return face;
}
} // namespace

HAOCAM_TEST(facelandmark_68_mapping_returns_anatomical_points) {
    FaceData face = canonicalFace();

    const Point2D leftEye = getLandmark(face, FaceLandmark::LeftEyeCenter);
    const Point2D rightEye = getLandmark(face, FaceLandmark::RightEyeCenter);
    const Point2D nose = getLandmark(face, FaceLandmark::NoseTip);
    const Point2D chin = getLandmark(face, FaceLandmark::Chin);

    HAOCAM_EXPECT(std::isfinite(leftEye.x));
    HAOCAM_EXPECT(std::isfinite(rightEye.x));
    HAOCAM_EXPECT(std::isfinite(nose.y));
    HAOCAM_EXPECT(std::isfinite(chin.y));
    HAOCAM_EXPECT(leftEye.x < rightEye.x); // subject's left eye on the left
    HAOCAM_EXPECT(nose.y > leftEye.y);     // nose below the eyes
    HAOCAM_EXPECT(chin.y > nose.y);        // chin below the nose
}

HAOCAM_TEST(getlandmark_returns_nan_when_missing) {
    FaceData face; // layout None, no landmarks
    const Point2D missing = getLandmark(face, FaceLandmark::NoseTip);
    HAOCAM_EXPECT(std::isnan(missing.x));
    HAOCAM_EXPECT(std::isnan(missing.y));
    HAOCAM_EXPECT(!hasLandmark(face, FaceLandmark::NoseTip));

    FaceData face68 = canonicalFace();
    HAOCAM_EXPECT(hasLandmark(face68, FaceLandmark::Chin));
    HAOCAM_EXPECT(hasLandmark(face68, FaceLandmark::LeftCheek));
    const Point2D chin = getLandmark(face68, FaceLandmark::Chin);
    HAOCAM_EXPECT(chin.y == 0.75f);
}

HAOCAM_TEST(facedata_bounds_follow_landmark_extents) {
    FaceData face;
    face.landmarks = {{0.2f, 0.3f}, {0.8f, 0.3f}, {0.5f, 0.1f}, {0.4f, 0.9f}};

    face.updateBoundsFromLandmarks();
    HAOCAM_EXPECT(std::abs(face.bounds.x - 0.2f) < 1e-5f);
    HAOCAM_EXPECT(std::abs(face.bounds.y - 0.1f) < 1e-5f);
    HAOCAM_EXPECT(std::abs(face.bounds.width - 0.6f) < 1e-5f);
    HAOCAM_EXPECT(std::abs(face.bounds.height - 0.8f) < 1e-5f);

    // Spec section 10: bounds are normalized [0,1].
    HAOCAM_EXPECT(face.bounds.x >= 0.0f && face.bounds.x + face.bounds.width <= 1.0f);
    HAOCAM_EXPECT(face.bounds.y >= 0.0f && face.bounds.y + face.bounds.height <= 1.0f);

    FaceData empty;
    empty.updateBoundsFromLandmarks(); // no-op, must not crash
    HAOCAM_EXPECT(empty.bounds.width == 0.0f);
}

HAOCAM_TEST(headpose_frontal_face_is_near_zero) {
    FaceData face = canonicalFace();
    const HeadPose pose = estimateHeadPose(face);
    HAOCAM_EXPECT(std::abs(pose.roll) < 2.0f);
    HAOCAM_EXPECT(std::abs(pose.yaw) < 5.0f);
    HAOCAM_EXPECT(std::abs(pose.pitch) < 8.0f);
}

HAOCAM_TEST(headpose_roll_top_toward_viewer_right_is_positive) {
    // Convention (FaceData.h): roll positive = top of head tilted toward the
    // viewer's right. Left eye up-left, right eye down-right => +45 degrees.
    FaceData face = canonicalFace();
    for (size_t i = 36; i <= 41; ++i) face.landmarks[i] = {0.40f, 0.40f};
    for (size_t i = 42; i <= 47; ++i) face.landmarks[i] = {0.60f, 0.60f};
    face.landmarks[k68NoseTip] = {0.50f, 0.50f};
    const HeadPose pose = estimateHeadPose(face);
    HAOCAM_EXPECT(std::abs(pose.roll - 45.0f) < 3.0f);
}

HAOCAM_TEST(headpose_yaw_positive_nose_toward_viewer_right) {
    // Convention (FaceData.h): yaw positive = face turned to THEIR left,
    // i.e. the nose projects toward the viewer's right of the eye midpoint.
    FaceData face = canonicalFace(0.56f); // nose right of midpoint 0.50
    const HeadPose pose = estimateHeadPose(face);
    HAOCAM_EXPECT(pose.yaw > 3.0f);

    FaceData mirrored = canonicalFace(0.44f); // nose left of midpoint
    const HeadPose mirroredPose = estimateHeadPose(mirrored);
    HAOCAM_EXPECT(mirroredPose.yaw < -3.0f);
}

HAOCAM_TEST(headpose_pitch_positive_when_looking_up) {
    // Nose high between forehead and chin => looking up => positive pitch.
    FaceData up = canonicalFace(0.50f, 0.38f);
    const HeadPose upPose = estimateHeadPose(up);
    HAOCAM_EXPECT(upPose.pitch > 3.0f);

    FaceData down = canonicalFace(0.50f, 0.62f);
    const HeadPose downPose = estimateHeadPose(down);
    HAOCAM_EXPECT(downPose.pitch < -3.0f);
}

HAOCAM_TEST(estimate_head_pose_ignores_undetected_faces) {
    FaceData face = canonicalFace();
    face.detected = false;
    const HeadPose pose = estimateHeadPose(face);
    HAOCAM_EXPECT(pose.yaw == 0.0f);
    HAOCAM_EXPECT(pose.pitch == 0.0f);
    HAOCAM_EXPECT(pose.roll == 0.0f);
}

HAOCAM_TEST(tracker_config_defaults_match_documentation) {
    FaceTrackerConfig config;
    HAOCAM_EXPECT_EQ(config.maxFaces, 1);
    HAOCAM_EXPECT_EQ(config.maxTrackingFps, 30.0f);
    HAOCAM_EXPECT(config.smoothingMinCutoff == 1.2f);
    HAOCAM_EXPECT(config.smoothingBeta == 0.007f);
}
