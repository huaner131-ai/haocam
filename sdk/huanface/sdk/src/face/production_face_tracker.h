/**
 * HuanFace Production Face Tracker — Phase 5.5 REAL ML
 * - Real ML pipeline: HFFrame -> Preprocess -> ONNX -> Detection -> Crop/ROI -> Landmark -> Postprocess -> HFFaceData
 * - Backend selection: AUTO/ONNX/HEURISTIC with explicit fallback warning
 * - Production mode requires ML, else FAIL init
 * - Temporal tracking AFTER ML inference: ML detection -> ML landmarks -> ML pose -> Temporal -> Smoothed
 */

#pragma once
#include "face_data.h"
#include "face_detector.h"
#include "landmark_estimator.h"
#include "face_mesh_generator.h"
#include "pose_estimator.h"
#include "tracking_state.h"
#include "inference_backend.h"
#include "../../include/huanface_c_api.h"
#include <memory>
#include <string>

namespace huanface {

class ProductionFaceTracker : public IFaceTracker {
public:
    ProductionFaceTracker();
    ~ProductionFaceTracker() override;

    HFResult Init(const HFEngineConfigC& config) override;
    void Shutdown() override;
    HFResult Process(const HFFrameC* input, HFTrackingData& outTracking) override;
    std::string GetName() const override { return "ProductionFaceTracker"; }

    // Config
    void SetMaxFaces(int max) { maxFaces = max; }
    void SetSmoothingAlpha(float alpha) { smoothingAlpha = alpha; }
    void SetBackendType(HFInferenceBackendType type) { backendType = type; }
    void SetProductionMode(bool prod) { productionMode = prod; }
    void SetCameraIntrinsics(const HFCameraIntrinsics& intr) { cameraIntrinsics = intr; }

    // For testing and debug
    IFaceInferenceBackend* GetBackend() { return inferenceBackend.get(); }
    TemporalTracker* GetTemporalTracker() { return temporalTracker.get(); }
    HFInferenceBackendType GetBackendType() const { return backendType; }
    bool IsProductionMode() const { return productionMode; }
    HFCameraIntrinsics GetCameraIntrinsics() const { return cameraIntrinsics; }

private:
    bool initialized = false;
    int maxFaces = 5;
    float smoothingAlpha = 0.6f;
    int imageWidth = 0, imageHeight = 0;
    HFInferenceBackendType backendType = HFInferenceBackendType::AUTO;
    bool productionMode = false;
    HFCameraIntrinsics cameraIntrinsics;

    // Components
    std::unique_ptr<IFaceInferenceBackend> inferenceBackend;
    std::unique_ptr<ProductionFaceDetector> faceDetector; // fallback direct detector
    std::unique_ptr<ProductionLandmarkEstimator> landmarkEstimator;
    std::unique_ptr<ProductionFaceMeshGenerator> meshGenerator;
    std::unique_ptr<ProductionPoseEstimator> poseEstimator;
    std::unique_ptr<TemporalTracker> temporalTracker;

    // For BGRA conversion
    std::vector<uint8_t> convertedBuffer;

    HFResult ProcessInternal(const uint8_t* rgba, int width, int height, int stride, int64_t timestamp,
                             HFTrackingData& outTracking);
    HFResult InitBackend(const HFEngineConfigC& config);
};

} // namespace huanface
