/**
 * HuanFace Pose Estimator — Phase 5 Production
 * Real yaw/pitch/roll from landmarks, not hardcoded
 */

#pragma once
#include "face_data.h"
#include "face_detector.h"
#include "../../include/huanface_c_api.h"
#include <vector>

namespace huanface {

class IPoseEstimator {
public:
    virtual ~IPoseEstimator() = default;
    virtual HFResult Init(const HFEngineConfigC& config) = 0;
    virtual void Shutdown() = 0;
    virtual HFResult Estimate(const std::vector<HFVec2>& landmarks,
                              const FaceDetection& face,
                              int imageWidth, int imageHeight,
                              HFFacePose& outPose) = 0;
    virtual std::string GetName() const = 0;
};

class ProductionPoseEstimator : public IPoseEstimator {
public:
    ProductionPoseEstimator() = default;
    ~ProductionPoseEstimator() override = default;

    HFResult Init(const HFEngineConfigC& config) override;
    void Shutdown() override;
    HFResult Estimate(const std::vector<HFVec2>& landmarks,
                      const FaceDetection& face,
                      int imageWidth, int imageHeight,
                      HFFacePose& outPose) override;
    std::string GetName() const override { return "ProductionPoseEstimator"; }

private:
    bool initialized = false;

    float ComputeRoll(const std::vector<HFVec2>& landmarks) const;
    float ComputeYaw(const std::vector<HFVec2>& landmarks, const FaceDetection& face) const;
    float ComputePitch(const std::vector<HFVec2>& landmarks, const FaceDetection& face) const;
};

} // namespace huanface
