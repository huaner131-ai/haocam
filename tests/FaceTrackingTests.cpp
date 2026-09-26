// Phase 2: tracker contract + null-provider degradation + result selection
// (spec sections 7-9, 14, 16).

#include <memory>
#include <string>

#include "face/FaceData.h"
#include "face/FaceTracker.h"
#include "frame/Frame.h"
#include "test_main.h"

using namespace haocam;

HAOCAM_TEST(null_tracker_reports_unavailable_and_never_tracks) {
    NullFaceTracker tracker;
    HAOCAM_EXPECT_EQ(std::string(tracker.name()), std::string("NullTracker"));
    HAOCAM_EXPECT(tracker.backend() == TrackerBackend::None);
    HAOCAM_EXPECT(!tracker.isAvailable());
    HAOCAM_EXPECT(tracker.statusText().find("Unavailable") != std::string::npos);

    HAOCAM_EXPECT(!tracker.initialize(FaceTrackerConfig{}));
    HAOCAM_EXPECT(tracker.statusText().find("Unavailable") != std::string::npos);

    Frame frame;
    frame.id = 42;
    frame.timestamp = 1000;
    const FaceTrackingResult result = tracker.process(frame);
    HAOCAM_EXPECT(!result.detected);
    HAOCAM_EXPECT(result.faces.empty());
    HAOCAM_EXPECT(result.confidence == 0.0f);
    HAOCAM_EXPECT(result.primary() == nullptr);
    HAOCAM_EXPECT(result.timestamp == 1000);
    tracker.shutdown();
}

HAOCAM_TEST(default_tracker_falls_back_without_mediapipe) {
    auto tracker = createDefaultTracker();
    HAOCAM_EXPECT(tracker != nullptr);
#ifdef HAOCAM_HAS_MEDIAPIPE
    HAOCAM_EXPECT(tracker->backend() == TrackerBackend::MediaPipe);
#else
    HAOCAM_EXPECT(tracker->backend() == TrackerBackend::None);
    HAOCAM_EXPECT_EQ(std::string(tracker->name()), std::string("NullTracker"));
#endif
}

HAOCAM_TEST(tracking_result_primary_returns_first_face_when_detected) {
    FaceTrackingResult result;
    result.detected = true;
    FaceData face;
    face.confidence = 0.8f;
    result.faces.push_back(face);

    const FaceData* primary = result.primary();
    HAOCAM_EXPECT(primary != nullptr);
    HAOCAM_EXPECT(primary->confidence == 0.8f);
    HAOCAM_EXPECT(!result.empty());
}

HAOCAM_TEST(tracking_result_primary_null_without_faces_or_detection) {
    FaceTrackingResult empty;
    HAOCAM_EXPECT(empty.primary() == nullptr);
    HAOCAM_EXPECT(!empty.detected);
    HAOCAM_EXPECT(empty.empty());

    FaceTrackingResult undetectedWithFace;
    undetectedWithFace.detected = false;
    undetectedWithFace.faces.emplace_back();
    HAOCAM_EXPECT(undetectedWithFace.primary() == nullptr);
}

HAOCAM_TEST(facedata_reset_clears_state) {
    FaceData face;
    face.detected = true;
    face.confidence = 0.7f;
    face.landmarks.assign(68, Point2D{0.5f, 0.5f});
    face.blendShapes.assign(52, 0.5f);
    face.reset();

    HAOCAM_EXPECT(!face.detected);
    HAOCAM_EXPECT(face.confidence == 0.0f);
    HAOCAM_EXPECT(face.landmarks.empty());
    HAOCAM_EXPECT(face.blendShapes.empty());
}
