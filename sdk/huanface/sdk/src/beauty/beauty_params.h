/**
 * Beauty Parameter System — Phase 7 Full Beauty & Face Retouching Engine
 * Centralized parameters, per-feature, affects rendering, testable sensitivity
 * NOT AI Beauty unless ML model exists — uses clean-room skin processing
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include "../makeup/makeup_params.h" // for HFFloat4 reuse
#include <string>
#include <map>

namespace huanface {

// ============================================================================
// Beauty Parameters — per feature
// ============================================================================

// Skin Smoothing
struct HFSkinSmoothingParams {
    bool enabled = true;
    float intensity = 0.5f; // 0-1, 0=no smoothing, 1=full
    float radius = 2.0f; // smoothing radius, 0.5-5.0
    float opacity = 0.8f; // 0-1 blend with original
    float edgePreservation = 0.6f; // 0-1, 0=gaussian, 1=edge-aware (bilateral-like)
};

// Skin Texture Refinement
struct HFSkinTextureParams {
    bool enabled = false;
    float intensity = 0.5f; // texture refinement strength
    float preservation = 0.7f; // structure preservation, avoid plastic
    float opacity = 0.7f;
    float detailThreshold = 0.3f; // threshold for small noise vs structure
};

// Blemish Reduction
struct HFBlemishReductionParams {
    bool enabled = false;
    float intensity = 0.5f;
    float radius = 2.5f;
    float opacity = 0.8f;
    // Note: NOT AI blemish detection, just skin blemish reduction via local smoothing + high-freq suppression
};

// Skin Tone Adjustment
struct HFSkinToneParams {
    bool enabled = false;
    float intensity = 0.5f;
    float temperature = 0.0f; // -1 to 1, warm/cool
    float tint = 0.0f; // -1 to 1, green/magenta
    float saturation = 0.0f; // -1 to 1, desat/sat
    float opacity = 0.7f;
};

// Brightness
struct HFBrightnessParams {
    bool enabled = false;
    float intensity = 0.0f; // -1 to 1, 0=neutral, range documented
    float opacity = 0.8f;
    bool skinOnly = true; // default skin-only, not global
};

// Contrast
struct HFContrastParams {
    bool enabled = false;
    float intensity = 0.0f; // -1 to 1, 0=neutral
    float opacity = 0.8f;
    bool skinOnly = true;
};

// Face Retouch — abstraction calling feature pipeline
struct HFBeautyRetouchParams {
    bool enabled = true;
    float intensity = 0.8f; // global
    float smoothing = 0.5f; // maps to smoothingIntensity
    float texture = 0.3f; // maps to textureIntensity
    float blemish = 0.4f;
    float tone = 0.3f;
    float brightness = 0.0f;
    float contrast = 0.0f;
    float opacity = 0.9f;
};

// ============================================================================
// Centralized Beauty Parameters — all features
// ============================================================================
struct HFBeautyParameters {
    bool enabled = true;
    float globalIntensity = 1.0f;
    float opacity = 0.9f;

    HFSkinSmoothingParams smoothing;
    HFSkinTextureParams texture;
    HFBlemishReductionParams blemish;
    HFSkinToneParams tone;
    HFBrightnessParams brightness;
    HFContrastParams contrast;
    HFBeautyRetouchParams retouch;

    bool IsValid() const {
        auto inRange01 = [](float v){ return v>=0.0f && v<=1.0f; };
        auto inRange11 = [](float v){ return v>=-1.0f && v<=1.0f; };
        if (!inRange01(smoothing.intensity) || !inRange01(smoothing.opacity)) return false;
        if (!inRange01(texture.intensity) || !inRange01(texture.preservation) || !inRange01(texture.opacity)) return false;
        if (!inRange01(blemish.intensity) || !inRange01(blemish.opacity)) return false;
        if (!inRange01(tone.intensity) || !inRange01(tone.opacity)) return false;
        if (!inRange11(tone.temperature) || !inRange11(tone.tint) || !inRange11(tone.saturation)) return false;
        if (!inRange11(brightness.intensity) || !inRange01(brightness.opacity)) return false;
        if (!inRange11(contrast.intensity) || !inRange01(contrast.opacity)) return false;
        if (!inRange01(retouch.intensity) || !inRange01(retouch.opacity)) return false;
        if (!inRange01(globalIntensity) || !inRange01(opacity)) return false;
        return true;
    }

    // For bundle integration
    std::map<std::string, float> ToFloatMap() const {
        std::map<std::string, float> m;
        m["beauty.enabled"] = enabled?1.0f:0.0f;
        m["beauty.globalIntensity"] = globalIntensity;
        m["beauty.opacity"] = opacity;
        m["beauty.smoothing.enabled"] = smoothing.enabled?1.0f:0.0f;
        m["beauty.smoothing.intensity"] = smoothing.intensity;
        m["beauty.smoothing.radius"] = smoothing.radius;
        m["beauty.smoothing.opacity"] = smoothing.opacity;
        m["beauty.smoothing.edgePreservation"] = smoothing.edgePreservation;
        m["beauty.texture.enabled"] = texture.enabled?1.0f:0.0f;
        m["beauty.texture.intensity"] = texture.intensity;
        m["beauty.texture.preservation"] = texture.preservation;
        m["beauty.texture.opacity"] = texture.opacity;
        m["beauty.blemish.enabled"] = blemish.enabled?1.0f:0.0f;
        m["beauty.blemish.intensity"] = blemish.intensity;
        m["beauty.blemish.radius"] = blemish.radius;
        m["beauty.blemish.opacity"] = blemish.opacity;
        m["beauty.tone.enabled"] = tone.enabled?1.0f:0.0f;
        m["beauty.tone.intensity"] = tone.intensity;
        m["beauty.tone.temperature"] = tone.temperature;
        m["beauty.tone.tint"] = tone.tint;
        m["beauty.tone.saturation"] = tone.saturation;
        m["beauty.tone.opacity"] = tone.opacity;
        m["beauty.brightness.enabled"] = brightness.enabled?1.0f:0.0f;
        m["beauty.brightness.intensity"] = brightness.intensity;
        m["beauty.brightness.opacity"] = brightness.opacity;
        m["beauty.contrast.enabled"] = contrast.enabled?1.0f:0.0f;
        m["beauty.contrast.intensity"] = contrast.intensity;
        m["beauty.contrast.opacity"] = contrast.opacity;
        m["beauty.retouch.enabled"] = retouch.enabled?1.0f:0.0f;
        m["beauty.retouch.intensity"] = retouch.intensity;
        m["beauty.retouch.smoothing"] = retouch.smoothing;
        m["beauty.retouch.texture"] = retouch.texture;
        m["beauty.retouch.blemish"] = retouch.blemish;
        m["beauty.retouch.tone"] = retouch.tone;
        m["beauty.retouch.brightness"] = retouch.brightness;
        m["beauty.retouch.contrast"] = retouch.contrast;
        m["beauty.retouch.opacity"] = retouch.opacity;
        return m;
    }
};

// ============================================================================
// Helpers
// ============================================================================
inline bool IsBeautyIntensityZero(float intensity) { return intensity <= 0.001f; }
inline bool IsBeautyIntensityFull(float intensity) { return intensity >= 0.999f; }

} // namespace huanface
