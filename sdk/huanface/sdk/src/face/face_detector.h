/**
 * HuanFace Face Detector — Phase 5 Production
 * Multi-face detection, improved over SimpleFaceTracker YCrCb only
 * Real detection via skin + edge + eye/mouth verification
 */

#pragma once
#include "face_data.h"
#include "../../include/huanface_c_api.h"
#include <vector>

namespace huanface {

struct FaceDetection {
    float x, y, w, h; // bbox pixel
    float confidence; // detection confidence [0,1]
    float eyeDistance; // estimated eye distance for scale
    bool hasEyes = false;
    bool hasMouth = false;
    // For tracking
    int id = -1;

    float IoU(const FaceDetection& other) const {
        float x1 = std::max(x, other.x);
        float y1 = std::max(y, other.y);
        float x2 = std::min(x+w, other.x+other.w);
        float y2 = std::min(y+h, other.y+other.h);
        float interW = x2 - x1;
        float interH = y2 - y1;
        if (interW <= 0 || interH <= 0) return 0.0f;
        float interArea = interW * interH;
        float area1 = w*h;
        float area2 = other.w*other.h;
        float unionArea = area1 + area2 - interArea;
        if (unionArea <= 0) return 0.0f;
        return interArea / unionArea;
    }

    bool IsValid() const { return w>0 && h>0 && confidence>0; }
};

class IFaceDetector {
public:
    virtual ~IFaceDetector() = default;
    virtual HFResult Init(const HFEngineConfigC& config) = 0;
    virtual void Shutdown() = 0;
    virtual HFResult Detect(const uint8_t* rgba, int width, int height, int stride,
                            std::vector<FaceDetection>& outFaces) = 0;
    virtual std::string GetName() const = 0;
};

// Production detector: improved heuristic with multi-face support
class ProductionFaceDetector : public IFaceDetector {
public:
    ProductionFaceDetector() = default;
    ~ProductionFaceDetector() override = default;

    HFResult Init(const HFEngineConfigC& config) override;
    void Shutdown() override;
    HFResult Detect(const uint8_t* rgba, int width, int height, int stride,
                    std::vector<FaceDetection>& outFaces) override;
    std::string GetName() const override { return "ProductionFaceDetector"; }

    void SetMinFaceRatio(float r) { minFaceRatio = r; }
    void SetMaxFaces(int m) { maxFaces = m; }
    void SetDetectSmallFace(bool v) { detectSmallFace = v; }

private:
    bool initialized = false;
    float minFaceRatio = 0.08f;
    int maxFaces = 5;
    bool detectSmallFace = false;

    bool IsSkinPixel(uint8_t r, uint8_t g, uint8_t b) const;
    bool VerifyFaceCandidate(const uint8_t* rgba, int width, int height, int stride,
                             const FaceDetection& candidate,
                             float& outEyeConfidence, float& outMouthConfidence) const;
    // Find dark regions (eyes)
    bool FindEyeCandidates(const uint8_t* rgba, int width, int height, int stride,
                           float faceX, float faceY, float faceW, float faceH,
                           HFVec2& leftEye, HFVec2& rightEye, float& confidence) const;
    bool FindMouthCandidate(const uint8_t* rgba, int width, int height, int stride,
                            float faceX, float faceY, float faceW, float faceH,
                            HFVec2& mouthCenter, float& confidence) const;
};

} // namespace huanface
