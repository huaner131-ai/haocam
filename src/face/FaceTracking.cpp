#include "face/FaceTracker.h"

#include <algorithm>

#include "core/logging/Logger.h"

#ifdef HAOCAM_HAS_MEDIAPIPE
#include "face/MediaPipeFaceTracker.h"
#endif

namespace haocam {

namespace {
constexpr const char* kCategory = "tracking";
}

const char* toStringTrackerBackend(TrackerBackend backend) {
    switch (backend) {
        case TrackerBackend::None: return "None";
        case TrackerBackend::MediaPipe: return "MediaPipe";
        case TrackerBackend::Provider: return "Provider";
    }
    return "Unknown";
}

bool NullFaceTracker::initialize(const FaceTrackerConfig& config) {
    (void)config;
    m_status = "Unavailable: no tracker backend in this build "
               "(place the MediaPipe SDK under sdk/mediapipe and enable "
               "HAOCAM_ENABLE_MEDIAPIPE)";
    HAOCAM_LOG_INFO(kCategory, "{}", m_status);
    return false;
}

void NullFaceTracker::shutdown() {}

FaceTrackingResult NullFaceTracker::process(const Frame& frame) {
    FaceTrackingResult result;
    result.timestamp = frame.timestamp;
    return result;
}

std::unique_ptr<IFaceTracker> createDefaultTracker() {
#ifdef HAOCAM_HAS_MEDIAPIPE
    auto tracker = std::make_unique<MediaPipeFaceTracker>();
    if (tracker->backendAvailable()) {
        return tracker;
    }
    // Backend missing (SDK present but runtime libs/model unavailable):
    // keep the tracker object so the UI can show its status reason.
    return tracker;
#else
    return std::make_unique<NullFaceTracker>();
#endif
}

} // namespace haocam
