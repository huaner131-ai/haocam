/**
 * HuanFace Makeup Mask System — Phase 6 Full Makeup Renderer
 * Semantic makeup masks from ML landmarks + face mesh
 * REAL landmark -> REAL mesh -> REAL mask (not random ellipse)
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
// HFMakeupMask — float alpha mask with feather/opacity/blur
// ============================================================================
struct HFMakeupMask {
    int width = 0;
    int height = 0;
    std::vector<float> alpha; // float 0-1 per pixel, not uint8
    float feather = 0.0f; // feather radius in pixels
    float blurRadius = 0.0f; // blur radius
    float opacity = 1.0f; // overall opacity

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
// Makeup Mask Types — Phase 6 required minimal
// ============================================================================
enum class MakeupMaskType {
    Face = 0,
    Lip,
    UpperLip,
    LowerLip,
    LeftEye,
    RightEye,
    LeftEyelid,
    RightEyelid,
    LeftEyebrow,
    RightEyebrow,
    LeftCheek,
    RightCheek,
    Nose,
    Count
};

inline std::string MakeupMaskTypeToString(MakeupMaskType t) {
    switch(t) {
        case MakeupMaskType::Face: return "Face";
        case MakeupMaskType::Lip: return "Lip";
        case MakeupMaskType::UpperLip: return "UpperLip";
        case MakeupMaskType::LowerLip: return "LowerLip";
        case MakeupMaskType::LeftEye: return "LeftEye";
        case MakeupMaskType::RightEye: return "RightEye";
        case MakeupMaskType::LeftEyelid: return "LeftEyelid";
        case MakeupMaskType::RightEyelid: return "RightEyelid";
        case MakeupMaskType::LeftEyebrow: return "LeftEyebrow";
        case MakeupMaskType::RightEyebrow: return "RightEyebrow";
        case MakeupMaskType::LeftCheek: return "LeftCheek";
        case MakeupMaskType::RightCheek: return "RightCheek";
        case MakeupMaskType::Nose: return "Nose";
        default: return "Unknown";
    }
}

// ============================================================================
// MakeupMaskGenerator — Phase 6 full
// Uses ML landmarks + face mesh, polygon/triangle rasterization, soft mask
// ============================================================================
class MakeupMaskGenerator {
public:
    MakeupMaskGenerator() = default;
    ~MakeupMaskGenerator() = default;

    // Main generation: face data + image size -> map of masks
    bool GenerateAllMasks(const HFFaceData& face, int imageWidth, int imageHeight,
                          std::map<MakeupMaskType, HFMakeupMask>& outMasks,
                          std::string& outError);

    // Single mask generation
    bool GenerateMask(const HFFaceData& face, MakeupMaskType type,
                      int imageWidth, int imageHeight,
                      HFMakeupMask& outMask, std::string& outError);

    // Specific masks
    bool GenerateFaceMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err);
    bool GenerateLipMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool upperOnly=false, bool lowerOnly=false);
    bool GenerateEyeMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool left=true, bool eyelidOnly=false);
    bool GenerateEyebrowMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool left=true);
    bool GenerateCheekMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err, bool left=true);
    bool GenerateNoseMask(const HFFaceData& face, int w, int h, HFMakeupMask& out, std::string& err);

    // Mask operations
    static void ApplyFeather(HFMakeupMask& mask, float featherRadius);
    static void ApplyBlur(HFMakeupMask& mask, float blurRadius);
    static void ApplyOpacity(HFMakeupMask& mask, float opacity);
    static void DilateMask(HFMakeupMask& mask, float radius);
    static void ErodeMask(HFMakeupMask& mask, float radius);

    // Helpers
    static bool PointInPolygon(float px, float py, const std::vector<HFVec2>& polygon);
    static bool PointInTriangle(float px, float py, const HFVec2& v0, const HFVec2& v1, const HFVec2& v2);
    static void RasterizePolygon(const std::vector<HFVec2>& polygon, HFMakeupMask& mask, float value=1.0f);
    static void RasterizeTriangle(const HFVec2& v0, const HFVec2& v1, const HFVec2& v2, HFMakeupMask& mask, float value=1.0f);
    static std::vector<HFVec2> GetLandmarkPolygon(const HFFaceData& face, const std::vector<int>& indices);
    static std::vector<int> GetLandmarkIndicesForMask(MakeupMaskType type);

    // Validation
    static bool ValidateMask(const HFMakeupMask& mask, std::string& outError);
    static bool ValidateMaskFollowsLandmarks(const HFFaceData& face1, const HFFaceData& face2,
                                             MakeupMaskType type, int w, int h);
};

} // namespace huanface
