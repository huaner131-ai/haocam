/**
 * Beauty Smoothing HLSL — Phase 10 parity with CPU BilateralLikeBlur
 * CPU: Bilateral ROI spatial exp(-dist²/2r²) * color exp(-colorDist²/2σ²) * mask
 *      blendFactor = maskAlpha * intensity * opacity
 */
#include "beauty_common.hlsl"

float4 PSSmoothing(PSInput input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 center = g_InputTexture.Sample(g_Sampler, uv);
    float skinMask = g_SkinMaskTexture.Sample(g_Sampler, uv).r;

    float intensity = g_SmoothingIntensity * g_BeautyOpacity * g_GlobalIntensity;
    if (intensity < 0.001f) return center;
    if (skinMask < 0.001f) return center;

    float radius = max(0.5f, g_SmoothingRadius);
    float2 texel = g_TexelSize * radius;

    float4 sum = center;
    float weightSum = 1.0f;

    float2 offsets[8] = {
        float2(-texel.x, -texel.y), float2(0, -texel.y), float2(texel.x, -texel.y),
        float2(-texel.x, 0),                        float2(texel.x, 0),
        float2(-texel.x, texel.y),  float2(0, texel.y),  float2(texel.x, texel.y)
    };

    [unroll]
    for (int i = 0; i < 8; ++i) {
        float2 sampleUV = uv + offsets[i];
        float4 sampleColor = g_InputTexture.Sample(g_Sampler, sampleUV);
        float colorDist = length(center.rgb - sampleColor.rgb);
        float edge = saturate(g_EdgePreservation);
        float sigmaColor = 0.15f + (1.0f - edge) * 0.35f;
        float colorWeight = exp(-colorDist * colorDist / (2.0f * sigmaColor * sigmaColor));
        float w = colorWeight;
        sum += sampleColor * w;
        weightSum += w;
    }

    float4 blurred = sum / weightSum;
    float blendFactor = saturate(skinMask * intensity * g_SmoothingOpacity);
    float4 result = lerp(center, blurred, blendFactor);
    return saturate(result);
}

float4 PSSmoothingGaussian(PSInput input) : SV_Target { return PSSmoothing(input); }
float4 PSMain(PSInput input) : SV_Target { return PSSmoothing(input); }
