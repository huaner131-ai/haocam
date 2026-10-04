/**
 * Makeup Parameter System — Phase 6 Full Makeup Renderer
 * Centralized parameters, per-feature, affects rendering, testable sensitivity
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include "../rendering/render_backend.h"
#include <string>
#include <map>

namespace huanface {

// ============================================================================
// HFFloat4 — RGBA float color
// ============================================================================
struct HFFloat4 {
    float r=1.0f, g=1.0f, b=1.0f, a=1.0f;
    HFFloat4(){}
    HFFloat4(float _r,float _g,float _b,float _a=1.0f):r(_r),g(_g),b(_b),a(_a){}
};

// ============================================================================
// HFBlendMode — Phase 6 blend abstraction (defined in render_backend.h)
// ============================================================================
// HFBlendMode is defined in render_backend.h as Normal/Multiply/Screen/Overlay per Phase 6 spec
// HFRenderBlendMode is rendering blend (OPAQUE/ALPHA_BLEND etc)

inline std::string BlendModeToString(HFBlendMode m) {
    switch(m){
        case HFBlendMode::Normal: return "Normal";
        case HFBlendMode::Multiply: return "Multiply";
        case HFBlendMode::Screen: return "Screen";
        case HFBlendMode::Overlay: return "Overlay";
        default: return "Unknown";
    }
}

// ============================================================================
// Per-feature makeup parameters — must affect rendering
// ============================================================================
struct HFLipMakeupParams {
    bool enabled = true;
    float intensity = 0.8f; // 0-1
    HFFloat4 color = HFFloat4(1.0f, 0.2f, 0.3f, 1.0f); // pinkish
    float opacity = 0.9f;
    float feather = 1.0f;
    float scale = 1.0f;
    HFBlendMode blendMode = HFBlendMode::Normal;
};

struct HFFoundationParams {
    bool enabled = false;
    float intensity = 0.5f;
    HFFloat4 color = HFFloat4(0.95f, 0.8f, 0.7f, 1.0f);
    float opacity = 0.6f;
    float feather = 2.0f;
    float softness = 0.5f;
    HFBlendMode blendMode = HFBlendMode::Normal;
};

struct HFBlushParams {
    bool enabled = false;
    float intensity = 0.6f;
    HFFloat4 color = HFFloat4(1.0f, 0.4f, 0.4f, 1.0f);
    float opacity = 0.7f;
    float feather = 3.0f;
    float scale = 1.0f;
    HFBlendMode blendMode = HFBlendMode::Normal;
};

struct HFEyebrowParams {
    bool enabled = false;
    float intensity = 0.7f;
    HFFloat4 color = HFFloat4(0.3f, 0.2f, 0.15f, 1.0f);
    float opacity = 0.8f;
    float feather = 1.5f;
    float thickness = 1.0f;
    HFBlendMode blendMode = HFBlendMode::Normal;
};

struct HFEyelinerParams {
    bool enabled = false;
    float intensity = 0.8f;
    HFFloat4 color = HFFloat4(0.1f, 0.1f, 0.1f, 1.0f);
    float opacity = 0.9f;
    float thickness = 2.0f;
    float feather = 0.5f;
    HFBlendMode blendMode = HFBlendMode::Normal;
};

struct HFEyelashParams {
    bool enabled = false;
    float intensity = 0.8f;
    float length = 1.0f;
    float thickness = 1.0f;
    float opacity = 0.9f;
    HFFloat4 color = HFFloat4(0.05f,0.05f,0.05f,1.0f);
};

struct HFEyeshadowParams {
    bool enabled = false;
    float intensity = 0.6f;
    HFFloat4 color = HFFloat4(0.8f, 0.4f, 0.6f, 1.0f);
    float opacity = 0.7f;
    float feather = 2.0f;
    HFBlendMode blendMode = HFBlendMode::Multiply;
};

struct HFPupilParams {
    bool enabled = false;
    float intensity = 0.5f;
    HFFloat4 color = HFFloat4(0.2f, 0.5f, 0.8f, 1.0f);
    float opacity = 0.6f;
    float irisEnhancement = 0.5f;
    float scale = 1.1f;
};

// ============================================================================
// Centralized makeup parameters — all features
// ============================================================================
struct HFMakeupParameters {
    HFLipMakeupParams lip;
    HFFoundationParams foundation;
    HFBlushParams blush;
    HFEyebrowParams eyebrow;
    HFEyelinerParams eyeliner;
    HFEyelashParams eyelash;
    HFEyeshadowParams eyeshadow;
    HFPupilParams pupil;

    // Global
    bool enabled = true;
    float globalIntensity = 1.0f;

    // Validation
    bool IsValid() const {
        // All intensities 0-1
        auto inRange = [](float v){ return v>=0.0f && v<=1.0f; };
        if (!inRange(lip.intensity) || !inRange(lip.opacity)) return false;
        if (!inRange(foundation.intensity) || !inRange(foundation.opacity)) return false;
        if (!inRange(blush.intensity) || !inRange(blush.opacity)) return false;
        if (!inRange(eyebrow.intensity) || !inRange(eyebrow.opacity)) return false;
        if (!inRange(eyeliner.intensity) || !inRange(eyeliner.opacity)) return false;
        if (!inRange(eyelash.intensity) || !inRange(eyelash.opacity)) return false;
        if (!inRange(eyeshadow.intensity) || !inRange(eyeshadow.opacity)) return false;
        if (!inRange(pupil.intensity) || !inRange(pupil.opacity)) return false;
        return true;
    }

    // For bundle integration
    std::map<std::string, float> ToFloatMap() const;
    std::map<std::string, HFFloat4> ToColorMap() const;
};

// ============================================================================
// Parameter sensitivity helpers
// ============================================================================
inline bool IsIntensityZero(float intensity) { return intensity <= 0.001f; }
inline bool IsIntensityFull(float intensity) { return intensity >= 0.999f; }

} // namespace huanface
