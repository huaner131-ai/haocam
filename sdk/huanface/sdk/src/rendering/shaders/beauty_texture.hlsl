/**
 * Beauty Texture Refinement HLSL — Phase 10 parity with CPU
 */
#include "beauty_common.hlsl"

float4 GaussianBlur9(float2 uv, float radius)
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
float4 PSTextureRefinement(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 orig = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float intensity = g_TextureIntensity * g_BeautyOpacity * g_GlobalIntensity;
    if (intensity < 0.001f) return orig;
    if (skinMask < 0.001f) return orig;
    float smallRadius = 1.0f;
    float largeRadius = intensity * 3.0f + 0.5f;
    float4 blurSmall = GaussianBlur9(uv, smallRadius);
    float4 blurLarge = GaussianBlur9(uv, largeRadius);
    float3 detail = orig.rgb - blurSmall.rgb;
    float detailLen = length(detail);
    float threshold = g_TextureDetailThreshold;
    float preservation = g_TexturePreservation;
    float suppression = 1.0f;
    if (detailLen < threshold) suppression = detailLen / max(threshold, 0.001f) * (1.0f - preservation);
    else suppression = 1.0f - preservation * 0.5f;
    suppression = saturate(suppression);
    float3 refined = blurLarge.rgb * intensity * 0.3f + blurSmall.rgb * (1.0f - intensity * 0.3f) + detail * suppression;
    float maskAlpha = skinMask * g_TextureOpacity * intensity;
    float3 result = orig.rgb * (1.0f - maskAlpha) + refined * maskAlpha;
    return saturate(float4(result, orig.a));
}
float4 PSMain(PSInput input) : SV_Target { return PSTextureRefinement(input); }
