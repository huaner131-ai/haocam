/**
 * HuanFace Makeup Engine Prototype — Phase 4
 * Input+Mask+Texture+Params -> Shader -> Composite
 * Lip color lerp(original, makeupColor, lipMask*intensity) clean-room
 */

#pragma once
#include "../face/face_data.h"
#include "face_mask.h"
#include "../../include/huanface/huanface_image.h"
#include "../../include/huanface_c_api.h"
#include <string>

namespace huanface {

struct MakeupParams {
    float intensityLip = 0.8f;
    HFColorC lipColor = {1.0f, 0.2f, 0.3f, 1.0f}; // pinkish
    // For future: blush, eyeshadow, etc
};

class MakeupEnginePrototype {
public:
    MakeupEnginePrototype() = default;
    ~MakeupEnginePrototype() = default;

    HFResult Init();
    void Shutdown();

    // Process: input RGBA + face + masks + params -> output RGBA
    // Clean-room lip shader: lerp(original, makeupColor, lipMask*intensity)
    HFResult Process(const HFImage& input, const HFFaceData& face, const FaceMask& lipMask, const MakeupParams& params, HFImage& outOutput, std::string& outError);

    // D3D11 version (if backend available): uses shader
    // For Phase 4 minimal, CPU version is used on Linux, D3D11 on Windows

    // Helpers
    static HFColorC LerpColor(const HFColorC& a, const HFColorC& b, float t);
    static uint8_t FloatToU8(float f) { return (uint8_t)std::max(0.0f, std::min(255.0f, f*255.0f)); }
};

} // namespace huanface
