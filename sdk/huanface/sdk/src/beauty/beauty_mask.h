/**
 * HuanFace Beauty Mask System — Phase 7 Full Beauty & Face Retouching Engine
 * Semantic beauty masks from ML landmarks + face mesh
 * REAL landmark -> REAL mesh -> REAL skin mask (not static ellipse)
 * Pipeline: Face Mesh -> Face Region -> Exclude Eyes -> Exclude Brows -> Exclude Lips -> Skin Mask
 */

#pragma once
#include "../face/face_data.h"
#include "../../include/huanface/huanface_image.h"
#include <string>
#include <vector>
#include <map>
#include <cmath>

namespace huanface {

// ============================================================================
// HFBeautyMask — float alpha mask with feather/blur/opacity
// ============================================================================
struct HFBeautyMask {
    int width = 0;
    int height = 0;
    std::vector<float> alpha; // float 0-1 per pixel
    float feather = 0.0f;
    float blurRadius = 0.0f;
    float opacity = 1.0f;

    bool IsValid() const {
        return width>0 && height>0 && alpha.size()==(size_t)width*height;
    }
    void Clear() { width=height=0; alpha.clear(); feather=0; blurRadius=0; opacity=1.0f; }
    float GetAlpha(int x, int y) const {
        if (x<0||x>=width||y<0||y>=height) return 0.0f;
        return alpha[y*width+x];
    }
    void SetAlpha(int x, int y, float v) {
        if (x<0||x>=width||y<0||y>=height) return;
        alpha[y*width+x] = std::max(0.0f, std::min(1.0f, v));
    }
    float MinAlpha() const {
        if (alpha.empty()) return 0.0f;
        float m=1.0f; for(float a:alpha) m=std::min(m,a); return m;
    }
    float MaxAlpha() const {
        if (alpha.empty()) return 0.0f;
        float m=0.0f; for(float a:alpha) m=std::max(m,a); return m;
    }
    bool HasFinite() const {
        for(float a:alpha) if (!std::isfinite(a)) return false;
        return true;
    }
    bool HasNonZero() const {
        for(float a:alpha) if (a>0.001f) return true;
        return false;
    }
    float Coverage() const {
        if (alpha.empty()) return 0.0f;
        float sum=0; for(float a:alpha) sum+=a;
        return sum / (float)alpha.size();
    }
};

// ============================================================================
// Beauty Mask Types — Phase 7 minimal + skin + exclusions
// ============================================================================
enum class BeautyMaskType {
    Face = 0,
    Forehead,
    LeftCheek,
    RightCheek,
    Nose,
    Chin,
    UnderEyeLeft,
    UnderEyeRight,
    Skin, // final skin mask after exclusions
    EyeExclusion,
    LipExclusion,
    BrowExclusion,
    Count
};

inline std::string BeautyMaskTypeToString(BeautyMaskType t) {
    switch(t) {
        case BeautyMaskType::Face: return "Face";
        case BeautyMaskType::Forehead: return "Forehead";
        case BeautyMaskType::LeftCheek: return "LeftCheek";
        case BeautyMaskType::RightCheek: return "RightCheek";
        case BeautyMaskType::Nose: return "Nose";
        case BeautyMaskType::Chin: return "Chin";
        case BeautyMaskType::UnderEyeLeft: return "UnderEyeLeft";
        case BeautyMaskType::UnderEyeRight: return "UnderEyeRight";
        case BeautyMaskType::Skin: return "Skin";
        case BeautyMaskType::EyeExclusion: return "EyeExclusion";
        case BeautyMaskType::LipExclusion: return "LipExclusion";
        case BeautyMaskType::BrowExclusion: return "BrowExclusion";
        default: return "Unknown";
    }
}

// ============================================================================
// HFBeautyMaskGenerator — Phase 7 full
// Uses ML landmarks + face mesh, polygon/triangle rasterization, exclusions
// ============================================================================
class HFBeautyMaskGenerator {
public:
    HFBeautyMaskGenerator() = default;
    ~HFBeautyMaskGenerator() = default;

    // Main generation: face data + image size -> map of masks
    bool GenerateAllMasks(const HFFaceData& face, int imageWidth, int imageHeight,
                          std::map<BeautyMaskType, HFBeautyMask>& outMasks,
                          std::string& outError);

    // Single mask generation
    bool GenerateMask(const HFFaceData& face, BeautyMaskType type,
                      int imageWidth, int imageHeight,
                      HFBeautyMask& outMask, std::string& outError);

    // Specific masks
    bool GenerateFaceMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateForeheadMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateCheekMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err, bool left=true);
    bool GenerateNoseMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateChinMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateUnderEyeMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err, bool left=true);
    bool GenerateSkinMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateEyeExclusionMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateLipExclusionMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);
    bool GenerateBrowExclusionMask(const HFFaceData& face, int w, int h, HFBeautyMask& out, std::string& err);

    // Mask operations
    static void ApplyFeather(HFBeautyMask& mask, float featherRadius);
    static void ApplyBlur(HFBeautyMask& mask, float blurRadius);
    static void ApplyOpacity(HFBeautyMask& mask, float opacity);
    static void DilateMask(HFBeautyMask& mask, float radius);
    static void ErodeMask(HFBeautyMask& mask, float radius);
    static void SubtractMask(HFBeautyMask& base, const HFBeautyMask& exclusion);
    static void IntersectMask(HFBeautyMask& base, const HFBeautyMask& other);

    // Helpers
    static bool PointInPolygon(float px, float py, const std::vector<HFVec2>& polygon);
    static bool PointInTriangle(float px, float py, const HFVec2& v0, const HFVec2& v1, const HFVec2& v2);
    static void RasterizePolygon(const std::vector<HFVec2>& polygon, HFBeautyMask& mask, float value=1.0f);
    static void RasterizeTriangle(const HFVec2& v0, const HFVec2& v1, const HFVec2& v2, HFBeautyMask& mask, float value=1.0f);
    static std::vector<HFVec2> GetLandmarkPolygon(const HFFaceData& face, const std::vector<int>& indices);
    static std::vector<int> GetLandmarkIndicesForMask(BeautyMaskType type);

    // Validation
    static bool ValidateMask(const HFBeautyMask& mask, std::string& outError);
    static bool ValidateMaskFollowsLandmarks(const HFFaceData& face1, const HFFaceData& face2,
                                             BeautyMaskType type, int w, int h);
    static bool ValidateSkinExclusion(const HFBeautyMask& skinMask, const HFBeautyMask& eyeMask, const HFBeautyMask& lipMask, std::string& err);
};

} // namespace huanface
