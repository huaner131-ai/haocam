/**
 * Makeup Blush HLSL — Phase 10 parity
 */
#include "makeup_common.hlsl"
float4 PSBlush(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float softness = blendParams.y;
    float softAlpha = mask * (1.0f - softness * 0.5f) + mask * mask * softness * 0.5f;
    return BlendWithMask(base, makeupColor, softAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSMain(PS_INPUT input) : SV_Target { return PSBlush(input); }
