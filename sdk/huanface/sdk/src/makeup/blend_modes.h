/**
 * Blend System — Phase 6 Full Makeup Renderer
 * Real blend formulas with alpha/mask consideration
 */

#pragma once
#include "makeup_params.h"
#include <cmath>
#include <algorithm>

namespace huanface {

// ============================================================================
// Blend formulas — real math, not fake
// ============================================================================
class BlendModes {
public:
    // Base and source are float 0-1, alpha 0-1, returns result 0-1
    static float BlendNormal(float base, float source, float alpha) {
        // result = source*alpha + base*(1-alpha) for normal with alpha
        // But per spec: result = source (when alpha considered separately)
        // We'll implement standard alpha blending: result = source*alpha + base*(1-alpha)
        return source * alpha + base * (1.0f - alpha);
    }

    static float BlendMultiply(float base, float source, float alpha) {
        // result = base * source
        // With alpha: result = (base*source)*alpha + base*(1-alpha)
        float blended = base * source;
        return blended * alpha + base * (1.0f - alpha);
    }

    static float BlendScreen(float base, float source, float alpha) {
        // result = 1 - (1-base)*(1-source)
        float blended = 1.0f - (1.0f - base)*(1.0f - source);
        return blended * alpha + base * (1.0f - alpha);
    }

    static float BlendOverlay(float base, float source, float alpha) {
        // Overlay: if base <0.5 => 2*base*source else 1-2*(1-base)*(1-source)
        float blended;
        if (base < 0.5f) {
            blended = 2.0f * base * source;
        } else {
            blended = 1.0f - 2.0f * (1.0f - base) * (1.0f - source);
        }
        blended = std::max(0.0f, std::min(1.0f, blended));
        return blended * alpha + base * (1.0f - alpha);
    }

    // Blend with mode enum
    static float Blend(float base, float source, float alpha, HFBlendMode mode) {
        switch(mode) {
            case HFBlendMode::Normal: return BlendNormal(base, source, alpha);
            case HFBlendMode::Multiply: return BlendMultiply(base, source, alpha);
            case HFBlendMode::Screen: return BlendScreen(base, source, alpha);
            case HFBlendMode::Overlay: return BlendOverlay(base, source, alpha);
            default: return BlendNormal(base, source, alpha);
        }
    }

    // Blend HFFloat4 colors
    static HFFloat4 BlendColor(const HFFloat4& base, const HFFloat4& source, float maskAlpha, HFBlendMode mode, float intensity, float opacity) {
        float alpha = maskAlpha * intensity * opacity * source.a;
        alpha = std::max(0.0f, std::min(1.0f, alpha));
        HFFloat4 result;
        result.r = Blend(base.r, source.r, alpha, mode);
        result.g = Blend(base.g, source.g, alpha, mode);
        result.b = Blend(base.b, source.b, alpha, mode);
        result.a = base.a; // keep base alpha
        return result;
    }

    // Lerp for reference
    static float Lerp(float a, float b, float t) {
        return a + (b-a)*t;
    }
};

} // namespace huanface
