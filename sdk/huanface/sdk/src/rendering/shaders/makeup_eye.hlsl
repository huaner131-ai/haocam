/**
 * Makeup Eye (eyeshadow/eyebrow/eyeliner/eyelash) HLSL — Phase 10 parity
 */
#include "makeup_common.hlsl"
float4 PSEyeshadow(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float softness = blendParams.y;
    float softAlpha = mask * (1.0f - softness * 0.5f) + mask * mask * softness * 0.5f;
    return BlendWithMask(base, makeupColor, softAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSEyebrow(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float thickness = blendParams.y;
    float thickAlpha = mask * saturate(thickness);
    return BlendWithMask(base, makeupColor, thickAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSEyeliner(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float thickness = blendParams.y;
    float thickAlpha = mask * saturate(thickness * 0.5f);
    return BlendWithMask(base, makeupColor, thickAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSEyelash(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float thickness = blendParams.y;
    float length = blendParams.z;
    float lashAlpha = mask * saturate(thickness) * saturate(length);
    return BlendWithMask(base, makeupColor, lashAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSMain(PS_INPUT input) : SV_Target { return PSEyeshadow(input); }
