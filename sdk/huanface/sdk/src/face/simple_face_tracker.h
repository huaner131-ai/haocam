/**
 * HuanFace Simple Face Tracker — Phase 4
 * Heuristic face detection: skin YCrCb + largest contour + bbox heuristics
 * Runtime-defined landmarks, not hardcoded 68/106
 * Clean-room, no proprietary
 */

#pragma once
#include "face_data.h"
#include "../../include/huanface_c_api.h"
#include <string>

namespace huanface {

class SimpleFaceTracker : public IFaceTracker {
public:
    SimpleFaceTracker() = default;
    ~SimpleFaceTracker() override = default;

    HFResult Init(const HFEngineConfigC& config) override;
    void Shutdown() override;
    HFResult Process(const HFFrameC* input, HFTrackingData& outTracking) override;
    std::string GetName() const override { return "SimpleFaceTracker"; }

    // Configurable
    void SetMinFaceRatio(float ratio) { minFaceRatio = ratio; }
    void SetMaxFaces(int max) { maxFaces = max; }

private:
    bool initialized = false;
    float minFaceRatio = 0.1f;
    int maxFaces = 5;
    int imageWidth = 0, imageHeight = 0;

    // Skin detection YCrCb
    bool IsSkinPixel(uint8_t r, uint8_t g, uint8_t b) const;
    // Find largest skin blob bbox
    bool FindFaceBBox(const uint8_t* rgba, int width, int height, int stride, float& outX, float& outY, float& outW, float& outH, float& outConfidence) const;
    // Generate landmarks from bbox
    void GenerateLandmarks(const HFFaceData& faceTemplate, int imageW, int imageH, HFFaceData& outFace) const;
    // Generate mesh
    void GenerateMesh(HFFaceData& face, int imageW, int imageH) const;
};

} // namespace huanface
