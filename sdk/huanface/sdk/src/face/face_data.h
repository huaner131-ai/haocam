/**
 * HuanFace Face Data — Phase 5 Production Tracking & Mesh
 * Extended from Phase 4 with real pose, confidence, tracking state, regions
 * Coordinate system documented in detail
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include <vector>
#include <string>
#include <cmath>

namespace huanface {

// ============================================================================
// Coordinate System Documentation (Phase 5)
// ============================================================================
// Image coordinate: origin top-left (0,0), X right, Y down, pixel units
//   - (0,0) = top-left corner of image
//   - (width-1, height-1) = bottom-right
//   - Used for: bbox, landmarks pixel, mesh vertices pixel
// Normalized coordinate: origin top-left, X right, Y down, [0,1] range
//   - u = x / width, v = y / height
//   - Used for: UV, mesh UV, normalized landmarks
// Face coordinate: origin at face center (bbox center), X right, Y down, Z out
//   - Right-handed: X right, Y down, Z out of screen (toward viewer)
//   - Used for: 3D landmarks relative depth
// Mesh coordinate: same as face coordinate but for mesh vertices
//   - Vertices in pixel space + depth, UV normalized [0,1]
// Screen coordinate: for rendering, origin top-left or center depending on API
//   - D3D11 NDC: (u*2-1, 1-v*2) => x in [-1,1], y in [-1,1], origin center
//   - OpenGL NDC similar
// D3D coordinate: left-handed or right-handed? HuanFace uses right-handed for logic,
//   converts to D3D left-handed in backend if needed via transform
// Rotation convention:
//   - Yaw: rotation around Y axis (vertical), positive = face turns right (left side more visible)
//   - Pitch: rotation around X axis (horizontal), positive = face looks down
//   - Roll: rotation around Z axis (depth), positive = clockwise tilt
//   - All in degrees, range: yaw [-90,90], pitch [-90,90], roll [-180,180]
// Translation: tx,ty in pixel (face center), tz depth estimated from bbox size
// Scale: uniform scale = bboxW / 200.0f (normalized to 200px reference)
// Landmark topology: 68 points standard (0-16 jaw, 17-21 right brow, 22-26 left brow,
//   27-30 nose bridge, 31-35 nose tip, 36-41 right eye, 42-47 left eye, 48-60 outer lip, 61-67 inner lip)
//   Plus 5-point base: left eye, right eye, nose, left mouth, right mouth
// ============================================================================

struct HFVec2 {
    float x, y;
    HFVec2() : x(0), y(0) {}
    HFVec2(float _x,float _y):x(_x),y(_y){}
};

struct HFVec3 {
    float x, y, z;
    HFVec3() : x(0), y(0), z(0) {}
    HFVec3(float _x,float _y,float _z):x(_x),y(_y),z(_z){}
};

struct HFVec2UV {
    float u, v;
    HFVec2UV():u(0),v(0){}
    HFVec2UV(float _u,float _v):u(_u),v(_v){}
};

// Face regions for mesh segmentation
enum class FaceRegion {
    FACE = 0,
    FOREHEAD = 1,
    LEFT_EYE = 2,
    RIGHT_EYE = 3,
    LEFT_BROW = 4,
    RIGHT_BROW = 5,
    NOSE = 6,
    NOSE_BRIDGE = 7,
    NOSE_TIP = 8,
    LIP = 9,
    OUTER_LIP = 10,
    INNER_LIP = 11,
    LEFT_CHEEK = 12,
    RIGHT_CHEEK = 13,
    CHIN = 14,
    JAW = 15,
    COUNT = 16
};

struct HFFacePose {
    float yaw = 0.0f;   // Y axis, degrees, [-90,90], positive right turn
    float pitch = 0.0f; // X axis, degrees, [-90,90], positive down
    float roll = 0.0f;  // Z axis, degrees, [-180,180], positive clockwise
    float tx = 0.0f;    // translation X pixel, face center
    float ty = 0.0f;    // translation Y pixel
    float tz = 0.0f;    // translation Z depth estimated
    float scale = 1.0f; // uniform scale relative to 200px reference

    bool IsValid() const {
        return std::isfinite(yaw) && std::isfinite(pitch) && std::isfinite(roll) &&
               std::isfinite(tx) && std::isfinite(ty) && std::isfinite(tz);
    }
};

enum class HFTrackingState {
    DETECTED = 0,   // newly detected this frame
    TRACKED = 1,    // tracked from previous frame
    LOST = 2,       // lost this frame, was tracked before
    REAPPEARED = 3  // reappeared after being lost
};

struct HFFaceMesh {
    std::vector<HFVec3> vertices; // pixel space + depth
    std::vector<int> indices; // triangles, 3 per triangle
    std::vector<HFVec2UV> uv; // normalized [0,1]
    std::vector<FaceRegion> vertexRegions; // region per vertex
    int width = 0;
    int height = 0;

    bool IsValid() const {
        if (vertices.empty() || indices.empty()) return false;
        if (vertices.size() != uv.size()) return false;
        if (indices.size() % 3 != 0) return false;
        // Check no NaN
        for (auto& v : vertices) {
            if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return false;
        }
        // Check indices in bounds
        for (int idx : indices) {
            if (idx < 0 || idx >= (int)vertices.size()) return false;
        }
        return true;
    }
    void Clear() { vertices.clear(); indices.clear(); uv.clear(); vertexRegions.clear(); width=height=0; }
    int VertexCount() const { return (int)vertices.size(); }
    int TriangleCount() const { return (int)indices.size()/3; }
};

enum class HFInferenceBackendType {
    AUTO = 0,      // Auto: use ONNX if available else HEURISTIC with warning
    ONNX = 1,      // ONNX Runtime real ML
    HEURISTIC = 2, // Fallback heuristic / development / CI / test only, NOT production ML
    MEDIAPIPE = 3  // Optional
};

struct HFCameraIntrinsics {
    float fx = 0.0f; // focal x, 0 = auto from image width
    float fy = 0.0f; // focal y, 0 = auto
    float cx = 0.0f; // principal point x, 0 = image center
    float cy = 0.0f; // principal point y
    bool IsDefault() const { return fx==0 && fy==0 && cx==0 && cy==0; }
    void SetDefaultFromImage(int width, int height) {
        if (fx==0) fx = (float)width;
        if (fy==0) fy = (float)width;
        if (cx==0) cx = (float)width * 0.5f;
        if (cy==0) cy = (float)height * 0.5f;
    }
};

struct HFFaceData {
    int id = 0; // persistent face ID for tracking
    float bboxX = 0, bboxY = 0, bboxW = 0, bboxH = 0;
    float confidence = 0.0f; // legacy combined confidence

    // Phase 5 extended confidence
    float detectionConfidence = 0.0f; // face detection confidence [0,1] from model or justified metric
    float landmarkConfidence = 0.0f;  // landmark estimation confidence [0,1]
    float trackingConfidence = 0.0f;  // temporal tracking confidence [0,1]

    std::vector<HFVec2> landmarks; // 2D pixel coordinates, size = landmarkCount (68 typical)
    std::vector<HFVec3> landmarks3D; // 3D with depth
    std::vector<float> landmarkConfidences; // per-landmark confidence [0,1]
    std::vector<HFVec2> landmarksNormalized; // normalized [0,1] version

    // Legacy rotation/translation/scale (kept for compatibility)
    float rotationPitch = 0, rotationYaw = 0, rotationRoll = 0;
    float translationX = 0, translationY = 0, translationZ = 0;
    float scale = 1.0f;

    // Phase 5 pose
    HFFacePose pose;

    // Tracking state
    HFTrackingState trackingState = HFTrackingState::DETECTED;
    int64_t lastSeenTimestamp = 0;
    int lostFrames = 0;

    HFFaceMesh mesh;

    bool IsValid() const { return bboxW>0 && bboxH>0 && confidence>0 && !landmarks.empty(); }

    // Coordinate conversions
    HFVec2UV LandmarkToUV(int idx, int imageWidth, int imageHeight) const {
        if (idx<0 || idx >= (int)landmarks.size()) return HFVec2UV(0,0);
        return HFVec2UV(landmarks[idx].x / (float)imageWidth, landmarks[idx].y / (float)imageHeight);
    }
    HFVec2 LandmarkToPixel(int idx) const {
        if (idx<0 || idx >= (int)landmarks.size()) return HFVec2(0,0);
        return landmarks[idx];
    }
    HFVec2 NormalizedToPixel(const HFVec2& norm, int imageWidth, int imageHeight) const {
        return HFVec2(norm.x * imageWidth, norm.y * imageHeight);
    }
    HFVec2 PixelToNormalized(const HFVec2& pix, int imageWidth, int imageHeight) const {
        return HFVec2(pix.x / (float)imageWidth, pix.y / (float)imageHeight);
    }
    // UV to D3D11 NDC: (u*2-1, 1-v*2)
    static HFVec2 UVToD3D11NDC(const HFVec2UV& uv) {
        return HFVec2(uv.u*2.0f-1.0f, 1.0f-uv.v*2.0f);
    }
    // Handle mirror
    HFVec2 ApplyMirror(const HFVec2& pt, int imageWidth) const {
        return HFVec2((float)imageWidth - 1 - pt.x, pt.y);
    }
    // Handle rotation 0/90/180/270
    HFVec2 ApplyRotation(const HFVec2& pt, int imageWidth, int imageHeight, int rotation) const {
        switch (rotation) {
            case 0: return pt;
            case 90: return HFVec2((float)imageHeight - 1 - pt.y, pt.x);
            case 180: return HFVec2((float)imageWidth - 1 - pt.x, (float)imageHeight - 1 - pt.y);
            case 270: return HFVec2(pt.y, (float)imageWidth - 1 - pt.x);
            default: return pt;
        }
    }
};

// Tracking data multi-face
struct HFTrackingData {
    std::vector<HFFaceData> faces;
    int64_t timestampNanos = 0;
    int FaceCount() const { return (int)faces.size(); }
    void Clear() { faces.clear(); timestampNanos=0; }
};

// Face tracker interface (preserved from Phase 3/4)
class IFaceTracker {
public:
    virtual ~IFaceTracker() = default;
    virtual HFResult Init(const HFEngineConfigC& config) = 0;
    virtual void Shutdown() = 0;
    virtual HFResult Process(const HFFrameC* input, HFTrackingData& outTracking) = 0;
    virtual std::string GetName() const = 0;
};

} // namespace huanface
