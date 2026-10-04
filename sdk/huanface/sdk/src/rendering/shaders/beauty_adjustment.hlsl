/**
 * Beauty Adjustment HLSL — Phase 10 parity with CPU reference
 */
#include "beauty_common.hlsl"

float4 PSBrightnessContrast(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 color = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float opacity = g_BeautyOpacity * g_GlobalIntensity;
    float maskAlpha = skinMask * opacity;
    float b = g_Brightness;
    float c = g_Contrast;
    if (abs(b) > 0.001f) color.rgb += b * maskAlpha * 0.5f;
    if (abs(c) > 0.001f) {
        float factor = 1.0f + c;
        float3 newV = (color.rgb - 0.5f) * factor + 0.5f;
        color.rgb = color.rgb * (1.0f - maskAlpha) + newV * maskAlpha;
    }
    return saturate(color);
}
float4 PSBrightness(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 color = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float opacity = g_BeautyOpacity * g_GlobalIntensity;
    float maskAlpha = skinMask * opacity;
    float b = g_Brightness;
    if (abs(b) > 0.001f) color.rgb += b * maskAlpha * 0.5f;
    return saturate(color);
}
float4 PSContrast(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 color = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float opacity = g_BeautyOpacity * g_GlobalIntensity;
    float maskAlpha = skinMask * opacity;
    float c = g_Contrast;
    if (abs(c) > 0.001f) {
        float factor = 1.0f + c;
        float3 newV = (color.rgb - 0.5f) * factor + 0.5f;
        color.rgb = color.rgb * (1.0f - maskAlpha) + newV * maskAlpha;
    }
    return saturate(color);
}
float4 PSBeautyFinal(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 color = SampleInput(uv);
    float skinMask = SampleSkinMask(uv);
    float opacity = g_BeautyOpacity * g_GlobalIntensity;
    float maskAlpha = skinMask * opacity;
    float b = g_Brightness;
    float c = g_Contrast;
    if (abs(b) > 0.001f) color.rgb += b * maskAlpha * 0.5f;
    if (abs(c) > 0.001f) {
        float factor = 1.0f + c;
        float3 newV = (color.rgb - 0.5f) * factor + 0.5f;
        color.rgb = color.rgb * (1.0f - maskAlpha) + newV * maskAlpha;
    }
    return saturate(color);
}
float4 PSMain(PSInput input) : SV_Target { return PSBrightness(input); }
