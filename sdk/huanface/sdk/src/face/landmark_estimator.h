/**
 * HuanFace Landmark Estimator — Phase 5 Production
 * Real landmarks from actual image analysis, not sin/cos synthetic
 */

#pragma once
#include "face_data.h"
#include "face_detector.h"
#include "../../include/huanface_c_api.h"
#include <vector>

namespace huanface {

class ILandmarkEstimator {
public:
    virtual ~ILandmarkEstimator() = default;
    virtual HFResult Init(const HFEngineConfigC& config) = 0;
    virtual void Shutdown() = 0;
    virtual HFResult Estimate(const uint8_t* rgba, int width, int height, int stride,
                              const FaceDetection& face,
                              std::vector<HFVec2>& outLandmarks,
                              std::vector<HFVec3>& outLandmarks3D,
                              std::vector<float>& outConfidences) = 0;
    virtual std::string GetName() const = 0;
};

// Production landmark estimator: detects eyes, nose, mouth from image, then builds 68 landmarks anchored to real detections
class ProductionLandmarkEstimator : public ILandmarkEstimator {
public:
    ProductionLandmarkEstimator() = default;
    ~ProductionLandmarkEstimator() override = default;

    HFResult Init(const HFEngineConfigC& config) override;
    void Shutdown() override;
    HFResult Estimate(const uint8_t* rgba, int width, int height, int stride,
                      const FaceDetection& face,
                      std::vector<HFVec2>& outLandmarks,
                      std::vector<HFVec3>& outLandmarks3D,
                      std::vector<float>& outConfidences) override;
    std::string GetName() const override { return "ProductionLandmarkEstimator"; }

private:
    bool initialized = false;

    // Real detection helpers (image reading, not synthetic)
    bool DetectEyeCenters(const uint8_t* rgba, int width, int height, int stride,
                          const FaceDetection& face,
                          HFVec2& leftEye, HFVec2& rightEye,
                          float& leftConf, float& rightConf) const;

    bool DetectNose(const uint8_t* rgba, int width, int height, int stride,
                    const FaceDetection& face,
                    const HFVec2& leftEye, const HFVec2& rightEye,
                    HFVec2& noseTip, float& conf) const;

    bool DetectMouth(const uint8_t* rgba, int width, int height, int stride,
                     const FaceDetection& face,
                     HFVec2& mouthCenter, HFVec2& leftCorner, HFVec2& rightCorner,
                     float& conf) const;

    bool DetectEyebrows(const uint8_t* rgba, int width, int height, int stride,
                        const FaceDetection& face,
                        const HFVec2& leftEye, const HFVec2& rightEye,
                        HFVec2& leftBrow, HFVec2& rightBrow,
                        float& leftConf, float& rightConf) const;

    // Build 68 landmarks from detected base points (real anchoring)
    void Build68Landmarks(const FaceDetection& face,
                          const HFVec2& leftEye, const HFVec2& rightEye,
                          const HFVec2& noseTip,
                          const HFVec2& mouthCenter, const HFVec2& leftMouth, const HFVec2& rightMouth,
                          const HFVec2& leftBrow, const HFVec2& rightBrow,
                          std::vector<HFVec2>& outLandmarks) const;

    // Depth estimation
    float EstimateDepth(const HFVec2& pt, const FaceDetection& face, const std::string& region) const;
};

} // namespace huanface
