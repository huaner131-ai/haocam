#include "face/FaceTracker.h"

#include "core/logging/Logger.h"

namespace haocam {

namespace {
constexpr const char* kCategory = "tracking";
}

bool NullFaceTracker::initialize() {
    HAOCAM_LOG_INFO(kCategory,
                    "Face tracking unavailable in this build (Phase 2); providers "
                    "will receive empty FaceData");
    return false;
}

void NullFaceTracker::shutdown() {}

bool NullFaceTracker::track(const Frame& frame, FaceData& result) {
    (void)frame;
    result.reset();
    return false;
}

std::unique_ptr<IFaceTracker> createDefaultTracker() {
    return std::make_unique<NullFaceTracker>();
}

} // namespace haocam
