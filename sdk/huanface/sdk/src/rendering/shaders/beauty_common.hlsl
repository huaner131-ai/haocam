/**
 * Beauty Common HLSL — Phase 7 Full Beauty Engine
 * Real HLSL with Texture2D, SamplerState, cbuffer, VSMain, common functions
 */

// Constant buffer for beauty parameters — must match CPU struct layout
cbuffer BeautyConstants : register(b0)
{
    float4 g_SkinColor; // not used, placeholder
    float g_SmoothingIntensity;
    float g_SmoothingRadius;
    float g_SmoothingOpacity;
    float g_EdgePreservation;

    float g_TextureIntensity;
    float g_TexturePreservation;
    float g_TextureOpacity;
    float g_TextureDetailThreshold;

    float g_BlemishIntensity;
    float g_BlemishRadius;
    float g_BlemishOpacity;
    float g_Pad0;

    float g_ToneIntensity;
    float g_ToneTemperature;
    float g_ToneTint;
    float g_ToneSaturation;

    float g_Brightness;
    float g_Contrast;
    float g_BeautyOpacity;
    float g_GlobalIntensity;

    float2 g_TexelSize; // 1/width, 1/height
    float2 g_Pad1;
};

// Textures
Texture2D g_InputTexture : register(t0);
Texture2D g_SkinMaskTexture : register(t1);
Texture2D g_IntermediateTexture : register(t2); // ping-pong
Texture2D g_BeautyMaskTexture : register(t3);

SamplerState g_Sampler : register(s0);
SamplerState g_PointSampler : register(s1);

// Vertex shader input/output
struct VSInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// Vertex shader main — full screen quad
PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = input.position; // fixed from float4(float3,1) to float4, quad now float4
    output.texcoord = input.texcoord;
    return output;
}

// Helper: sample input with mask consideration
float4 SampleInput(float2 uv)
{
    return g_InputTexture.Sample(g_Sampler, uv);
}

float SampleSkinMask(float2 uv)
{
    return g_SkinMaskTexture.Sample(g_Sampler, uv).r;
}

// Edge preservation helper — bilateral-like weight
float ComputeBilateralWeight(float4 centerColor, float4 sampleColor, float2 offset, float radius, float edgePreservation)
{
    float spatialDist = length(offset);
    float spatialWeight = exp(-spatialDist * spatialDist / (2.0f * radius * radius));

    float colorDist = length(centerColor.rgb - sampleColor.rgb);
    float sigmaColor = 0.1f + (1.0f - edgePreservation) * 0.4f;
    float colorWeight = exp(-colorDist * colorDist / (2.0f * sigmaColor * sigmaColor));

    return spatialWeight * colorWeight;
}

// Tone mapping helpers
float3 AdjustTemperature(float3 color, float temperature)
{
    // Warm: increase R, decrease B; Cool: opposite
    if (temperature > 0)
    {
        color.r += temperature * 0.2f;
        color.b -= temperature * 0.1f;
    }
    else
    {
        color.r += temperature * 0.1f;
        color.b -= temperature * 0.2f;
    }
    return color;
}

float3 AdjustTint(float3 color, float tint)
{
    if (tint > 0)
    {
        color.g -= tint * 0.1f;
    }
    else
    {
        color.g += (-tint) * 0.1f;
    }
    return color;
}

float3 AdjustSaturation(float3 color, float saturation)
{
    float luma = dot(color, float3(0.299f, 0.587f, 0.114f));
    return luma + (color - luma) * (1.0f + saturation);
}

// Brightness/Contrast
float3 AdjustBrightness(float3 color, float brightness)
{
    return color + brightness * 0.5f;
}

float3 AdjustContrast(float3 color, float contrast)
{
    float factor = 1.0f + contrast;
    return (color - 0.5f) * factor + 0.5f;
}
