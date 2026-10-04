/**
 * HuanFace Makeup Engine Prototype Implementation — Phase 4
 */

#include "makeup_engine.h"
#include <algorithm>
#include <cmath>

namespace huanface {

HFResult MakeupEnginePrototype::Init() {
    return HF_RESULT_OK;
}

void MakeupEnginePrototype::Shutdown() {}

HFColorC MakeupEnginePrototype::LerpColor(const HFColorC& a, const HFColorC& b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    HFColorC res;
    res.r = a.r + (b.r - a.r)*t;
    res.g = a.g + (b.g - a.g)*t;
    res.b = a.b + (b.b - a.b)*t;
    res.a = a.a + (b.a - a.a)*t;
    return res;
}

HFResult MakeupEnginePrototype::Process(const HFImage& input, const HFFaceData& face, const FaceMask& lipMask, const MakeupParams& params, HFImage& outOutput, std::string& outError) {
    (void)face; // For Phase 4, face used for mask generation already done outside
    if (!input.IsValid()) {
        outError = "Invalid input image for makeup";
        return HF_RESULT_INVALID_PARAM;
    }
    if (!lipMask.IsValid()) {
        outError = "Invalid lip mask for makeup";
        return HF_RESULT_INVALID_PARAM;
    }
    if (lipMask.width != input.width || lipMask.height != input.height) {
        outError = "Lip mask dimensions mismatch input";
        return HF_RESULT_INVALID_PARAM;
    }

    outOutput.width = input.width;
    outOutput.height = input.height;
    outOutput.channels = 4;
    outOutput.data.resize(input.data.size());

    float intensity = std::max(0.0f, std::min(1.0f, params.intensityLip));
    HFColorC makeupColor = params.lipColor;

    // CPU lerp: for each pixel where lipMask>0, blend
    for (int y=0; y<input.height; ++y) {
        for (int x=0; x<input.width; ++x) {
            size_t idx = (size_t)(y*input.width + x);
            uint8_t maskVal = lipMask.data[idx];
            float maskF = maskVal / 255.0f;
            float blendT = maskF * intensity;

            const uint8_t* srcPx = &input.data[idx*4];
            uint8_t* dstPx = &outOutput.data[idx*4];

            if (blendT <= 0.001f) {
                // No makeup
                dstPx[0]=srcPx[0];
                dstPx[1]=srcPx[1];
                dstPx[2]=srcPx[2];
                dstPx[3]=srcPx[3];
            } else {
                // Original color as float 0-1
                HFColorC orig;
                orig.r = srcPx[0]/255.0f;
                orig.g = srcPx[1]/255.0f;
                orig.b = srcPx[2]/255.0f;
                orig.a = srcPx[3]/255.0f;

                HFColorC blended = LerpColor(orig, makeupColor, blendT);

                dstPx[0]=FloatToU8(blended.r);
                dstPx[1]=FloatToU8(blended.g);
                dstPx[2]=FloatToU8(blended.b);
                dstPx[3]=srcPx[3]; // keep alpha
            }
        }
    }

    return HF_RESULT_OK;
}

} // namespace huanface
