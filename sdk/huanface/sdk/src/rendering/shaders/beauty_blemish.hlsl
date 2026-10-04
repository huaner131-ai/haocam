/**
 * Beauty Blemish Removal HLSL — Phase 10 parity with CPU
 */
#include "beauty_common.hlsl"

float4 GaussianBlur9B(float2 uv, float radius)
{
    float2 texel = g_TexelSize * radius;
    float4 sum = SampleInput(uv) * 4.0f;
    sum += SampleInput(uv + float2(-texel.x, 0)) * 2.0f;
    sum += SampleInput(uv + float2(texel.x, 0)) * 2.0f;
    sum += SampleInput(uv + float2(0, -texel.y)) * 2.0f;
    sum += SampleInput(uv + float2(0, texel.y)) * 2.0f;
    sum += SampleInput(uv + float2(-texel.x, -texel.y));
    sum += SampleInput(uv + float2(texel.x, -texel.y));
    sum += SampleInput(uv + float2(-texel.x, texel.y));
    sum += SampleInput(uv + float2(texel.x, texel.y));
    return sum / 16.0f;
}
float4 PSBlemishReduction(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 orig = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float intensity = g_BlemishIntensity * g_BeautyOpacity * g_GlobalIntensity;
    if (intensity < 0.001f) return orig;
    if (skinMask < 0.001f) return orig;
    float smallR = 1.0f;
    float largeR = max(0.5f, g_BlemishRadius);
    float4 blurSmall = GaussianBlur9B(uv, smallR);
    float4 blurLarge = GaussianBlur9B(uv, largeR);
    float3 highFreq = orig.rgb - blurSmall.rgb;
    float hfLen = length(highFreq);
    float blemishFactor = saturate(hfLen * 3.0f) * skinMask;
    float3 cleaned = orig.rgb * (1.0f - blemishFactor) + blurLarge.rgb * blemishFactor;
    float maskAlpha = skinMask * g_BlemishOpacity * intensity;
    float3 result = orig.rgb * (1.0f - maskAlpha) + cleaned * maskAlpha;
    return saturate(float4(result, orig.a));
}
float4 PSBlemishRemoval(PSInput input) : SV_Target { return PSBlemishReduction(input); }
float4 PSMain(PSInput input) : SV_Target { return PSBlemishReduction(input); }
