#include "face/MediaPipeFaceTracker.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "core/logging/Logger.h"

// ---------------------------------------------------------------------------
// MediaPipe Tasks C++ headers (official, compiled only when the SDK drop-in
// exists - see CMake option HAOCAM_ENABLE_MEDIAPIPE and sdk/mediapipe/README).
// API verified against mediapipe master (Jan 2026):
//   face_landmarker.h  : FaceLandmarkerOptions, FaceLandmarker::Create,
//                        DetectForVideo(Image, int64_t)
//   face_landmarker_result.h : FaceLandmarkerResult
//   components/containers/landmark.h : NormalizedLandmark {x,y,z,visibility,presence}
// ---------------------------------------------------------------------------
#include "mediapipe/framework/formats/image.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/tasks/cc/components/containers/landmark.h"
#include "mediapipe/tasks/cc/core/base_options.h"
#include "mediapipe/tasks/cc/vision/core/running_mode.h"
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarker.h"

namespace haocam {

namespace {
constexpr const char* kCategory = "tracking";
} // namespace

class MediaPipeFaceTracker::Impl {
public:
    FaceTrackerConfig config;
    std::unique_ptr<mediapipe::tasks::vision::face_landmarker::FaceLandmarker>
        landmarker;
    bool available = false;
    std::string status = "Not initialized";
    int64_t lastTimestampMs = -1;
    std::vector<float> blendScratch; // reusable blendshape buffer
};

MediaPipeFaceTracker::MediaPipeFaceTracker() : m_impl(std::make_unique<Impl>()) {}

MediaPipeFaceTracker::~MediaPipeFaceTracker() { shutdown(); }

bool MediaPipeFaceTracker::backendAvailable() const {
    // The tasks runtime is linked (not dynamically probed): availability
    // depends on the model asset, checked in initialize().
    return true;
}

bool MediaPipeFaceTracker::initialize(const FaceTrackerConfig& config) {
    shutdown();
    m_impl->config = config;

    if (config.modelAssetPath.empty()) {
        m_impl->status = "Unavailable: no model configured (place face_landmarker.task "
                         "under assets/models/ - never downloaded at runtime)";
        HAOCAM_LOG_WARN(kCategory, "{}", m_impl->status);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(config.modelAssetPath, ec)) {
        m_impl->status =
            "Unavailable: model file not found at " + config.modelAssetPath;
        HAOCAM_LOG_WARN(kCategory, "{}", m_impl->status);
        return false;
    }

    using mediapipe::tasks::vision::core::RunningMode;
    using mediapipe::tasks::vision::face_landmarker::FaceLandmarker;
    using mediapipe::tasks::vision::face_landmarker::FaceLandmarkerOptions;

    auto options = std::make_unique<FaceLandmarkerOptions>();
    options->base_options.model_asset_path = config.modelAssetPath;
    // CPU delegate: the supported desktop configuration for the tasks API.
    options->running_mode = RunningMode::VIDEO;
    options->num_faces = std::clamp(config.maxFaces, 1, 4);
    options->min_face_detection_confidence = config.minDetectionConfidence;
    options->min_face_presence_confidence = config.minPresenceConfidence;
    options->min_tracking_confidence = config.minTrackingConfidence;
    options->output_face_blendshapes = config.outputBlendShapes;
    options->output_facial_transformation_matrixes = config.outputHeadPoseMatrix;

    auto created = FaceLandmarker::Create(std::move(options));
    if (!created.ok()) {
        m_impl->status = "Initialization failed: " + std::string(created.status().message());
        HAOCAM_LOG_ERROR(kCategory, "FaceLandmarker::Create failed: {}", m_impl->status);
        return false;
    }

    m_impl->landmarker = std::move(created.value());
    m_impl->available = true;
    m_impl->status = "Ready";
    m_impl->lastTimestampMs = -1;
    HAOCAM_LOG_INFO(kCategory, "FaceTracker initialized (MediaPipe, model={})",
                    config.modelAssetPath);
    HAOCAM_LOG_INFO(kCategory, "FaceTracker model loaded: {}", config.modelAssetPath);
    return true;
}

void MediaPipeFaceTracker::shutdown() {
    if (m_impl->landmarker) {
        m_impl->landmarker->Close();
        m_impl->landmarker.reset();
    }
    m_impl->available = false;
    m_impl->status = "Not initialized";
}

bool MediaPipeFaceTracker::isAvailable() const { return m_impl->available; }

std::string MediaPipeFaceTracker::statusText() const { return m_impl->status; }

FaceTrackingResult MediaPipeFaceTracker::process(const Frame& frame) {
    // Frame-level entry point exists for the spec API; pixels arrive via the
    // tracking worker through processRGBA().
    FaceTrackingResult result;
    result.timestamp = frame.timestamp;
    return result;
}

FaceTrackingResult MediaPipeFaceTracker::processRGBA(const TrackingFrameView& view) {
    FaceTrackingResult result;
    if (!m_impl->available || !m_impl->landmarker || !view.rgba ||
        view.width == 0 || view.height == 0) {
        return result;
    }

    using mediapipe::Image;
    using mediapipe::ImageFrame;
    using mediapipe::ImageFormat;

    // Non-owning image: the tracking worker owns the buffer.
    auto frame = std::make_shared<ImageFrame>(
        ImageFormat::FORMAT_SRGBA, static_cast<int>(view.width),
        static_cast<int>(view.height), static_cast<int>(view.strideBytes),
        const_cast<uint8_t*>(view.rgba), [](uint8_t*) {});
    Image image(frame);

    // Monotonic millisecond timestamps (VIDEO running mode requirement).
    const int64_t timestampMs = static_cast<int64_t>(view.timestampUs / 1000);
    if (timestampMs <= m_impl->lastTimestampMs) {
        m_impl->lastTimestampMs += 1; // guarantee monotonicity
    } else {
        m_impl->lastTimestampMs = timestampMs;
    }

    auto detected = m_impl->landmarker->DetectForVideo(image, m_impl->lastTimestampMs);
    if (!detected.ok()) {
        HAOCAM_LOG_DEBUG(kCategory, "DetectForVideo failed: {}",
                         std::string(detected.status().message()));
        result.timestamp = view.timestampUs;
        return result;
    }

    const auto& mpResult = detected.value();
    result.timestamp = view.timestampUs;

    const size_t faceCount = std::min<size_t>(mpResult.face_landmarks.size(),
                                              static_cast<size_t>(m_impl->config.maxFaces));
    result.faces.resize(faceCount);

    for (size_t f = 0; f < faceCount; ++f) {
        FaceData& face = result.faces[f];
        const auto& mpLandmarks = mpResult.face_landmarks[f];

        face.landmarks.resize(mpLandmarks.size());
        float presenceSum = 0.0f;
        for (size_t i = 0; i < mpLandmarks.size(); ++i) {
            face.landmarks[i].x = mpLandmarks[i].x;
            face.landmarks[i].y = mpLandmarks[i].y;
            presenceSum += mpLandmarks[i].presence;
        }
        face.landmarkLayout = LandmarkLayout::MediaPipe468;
        face.confidence = mpLandmarks.empty()
                              ? 0.0f
                              : presenceSum / static_cast<float>(mpLandmarks.size());
        face.detected = true;
        face.updateBoundsFromLandmarks();

        // Blend shapes (optional output) - reusable buffer, no per-frame alloc.
        if (m_impl->config.outputBlendShapes && mpResult.face_blendshapes.has_value() &&
            f < mpResult.face_blendshapes.value().size()) {
            const auto& classifications = mpResult.face_blendshapes.value()[f];
            m_impl->blendScratch.clear();
            m_impl->blendScratch.reserve(classifications.categories.size());
            for (const auto& category : classifications.categories) {
                m_impl->blendScratch.push_back(category.score);
            }
            face.blendShapes = m_impl->blendScratch;
        }

        // Head pose: prefer the facial transformation matrix (when enabled),
        // fall back to the geometric estimate.
        face.headPose = face::estimateHeadPose(face);
        if (m_impl->config.outputHeadPoseMatrix &&
            mpResult.facial_transformation_matrixes.has_value() &&
            f < mpResult.facial_transformation_matrixes.value().size()) {
            const auto& matrix = mpResult.facial_transformation_matrixes.value()[f];
            if (matrix.rows() >= 3 && matrix.cols() >= 3) {
                // Row-major 4x4 rotation part -> Tait-Bryan angles.
                const float r00 = matrix(0, 0), r10 = matrix(1, 0), r20 = matrix(2, 0);
                const float r21 = matrix(2, 1), r22 = matrix(2, 2);
                const float sy = std::sqrt(r00 * r00 + r10 * r10);
                if (sy > 1e-5f) {
                    // MediaPipe canonical->camera conventions; mapped to
                    // HeadPose conventions documented in FaceData.h.
                    const float pitch = std::atan2(r21, r22);           // up/down
                    const float yaw = std::atan2(-r20, sy);             // left/right
                    const float roll =
                        std::atan2(r10, r00);                           // tilt
                    face.headPose.pitch = pitch * 180.0f / 3.14159265358979f;
                    face.headPose.yaw = -yaw * 180.0f / 3.14159265358979f;
                    face.headPose.roll = -roll * 180.0f / 3.14159265358979f;
                }
            }
        }
    }

    if (faceCount > 0) {
        result.detected = true;
        result.confidence = result.faces.front().confidence;
    }
    return result;
}

} // namespace haocam
