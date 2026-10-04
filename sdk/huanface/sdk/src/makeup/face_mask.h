/**
 * HuanFace Face Mask Generator — Phase 4
 * HFFaceMesh -> R8 mask triangle rasterization GPU texture
 * FeatureMask architecture: FaceMask/LipMask/EyeMask etc
 * Phase 4 only Face+Lip with data/face_regions/ mapping not proprietary
 */

#pragma once
#include "../face/face_data.h"
#include "../../include/huanface/huanface_image.h"
#include <string>
#include <vector>
#include <map>

namespace huanface {

enum class FeatureMaskType {
    FACE = 0,
    LIP = 1,
    EYE_LEFT = 2,
    EYE_RIGHT = 3,
    EYEBROW_LEFT = 4,
    EYEBROW_RIGHT = 5,
    NOSE = 6
};

struct FaceMask {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data; // R8, 0-255, 0=outside, 255=inside
    bool IsValid() const { return width>0 && height>0 && data.size()==(size_t)width*height; }
    void Clear() { width=height=0; data.clear(); }
};

class FaceMaskGenerator {
public:
    FaceMaskGenerator() = default;
    ~FaceMaskGenerator() = default;

    // Generate face mask from mesh: triangle rasterization R8
    bool GenerateFaceMask(const HFFaceMesh& mesh, int imageWidth, int imageHeight, FaceMask& outMask, std::string& outError);

    // Generate feature masks (lip, eye, etc) from landmarks
    // For Phase 4: only FACE and LIP implemented, others return empty
    bool GenerateFeatureMask(const HFFaceData& face, FeatureMaskType type, int imageWidth, int imageHeight, FaceMask& outMask, std::string& outError);

    // Helper: rasterize triangles into R8 mask
    static void RasterizeTriangle(const HFVec2& v0, const HFVec2& v1, const HFVec2& v2, FaceMask& mask, uint8_t value=255);

    // Helper: check if point in triangle barycentric
    static bool PointInTriangle(float px, float py, const HFVec2& v0, const HFVec2& v1, const HFVec2& v2);

    // Data/face_regions mapping: not proprietary, simple bbox proportion mapping
    // Returns landmark indices for feature
    static std::vector<int> GetFeatureLandmarkIndices(FeatureMaskType type);
};

} // namespace huanface
