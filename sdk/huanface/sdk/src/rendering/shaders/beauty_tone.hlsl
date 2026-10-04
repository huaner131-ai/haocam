/**
 * Beauty Tone Adjustment HLSL — Phase 10 parity with CPU reference
 */
#include "beauty_common.hlsl"

float4 PSToneAdjustment(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 color = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float intensity = g_ToneIntensity * g_BeautyOpacity * g_GlobalIntensity;
    if (intensity < 0.001f) return color;
    if (skinMask < 0.001f) return color;
    float maskAlpha = skinMask * g_BeautyOpacity * intensity;
    float3 rgb = color.rgb;
    if (abs(g_ToneTemperature) > 0.001f) rgb = AdjustTemperature(rgb, g_ToneTemperature * maskAlpha);
    if (abs(g_ToneTint) > 0.001f) rgb = AdjustTint(rgb, g_ToneTint * maskAlpha);
    if (abs(g_ToneSaturation) > 0.001f) {
        float luma = dot(rgb, float3(0.299f, 0.587f, 0.114f));
        float satFactor = 1.0f + g_ToneSaturation * maskAlpha;
        rgb = luma + (rgb - luma) * satFactor;
    }
    color.rgb = rgb;
    return saturate(color);
}
float4 PSMain(PSInput input) : SV_Target { return PSToneAdjustment(input); }
