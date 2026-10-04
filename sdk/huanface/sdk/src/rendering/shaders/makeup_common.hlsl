// makeup_common.hlsl — HuanFace Phase 6 Full Makeup Renderer
// Real HLSL, not fake, used by D3D11 backend

cbuffer MakeupConstants : register(b0)
{
    float4 makeupColor;
    float makeupIntensity;
    float makeupOpacity;
    float makeupFeather;
    float makeupScale;
    float4 blendParams; // x=blendMode, y=thickness, z=irisEnhance, w=scale
};

Texture2D inputTexture : register(t0);
Texture2D maskTexture : register(t1);
Texture2D makeupTexture : register(t2);
SamplerState samplerLinear : register(s0);

struct VS_INPUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = input.pos;
    output.uv = input.uv;
    return output;
}

// Blend functions — real math per Phase 6 spec
float3 BlendNormal(float3 base, float3 blend)
{
    return blend;
}

float3 BlendMultiply(float3 base, float3 blend)
{
    return base * blend;
}

float3 BlendScreen(float3 base, float3 blend)
{
    return 1.0f - (1.0f - base) * (1.0f - blend);
}

float3 BlendOverlay(float3 base, float3 blend)
{
    float3 result;
    result.r = (base.r < 0.5f) ? (2.0f * base.r * blend.r) : (1.0f - 2.0f * (1.0f - base.r) * (1.0f - blend.r));
    result.g = (base.g < 0.5f) ? (2.0f * base.g * blend.g) : (1.0f - 2.0f * (1.0f - base.g) * (1.0f - blend.g));
    result.b = (base.b < 0.5f) ? (2.0f * base.b * blend.b) : (1.0f - 2.0f * (1.0f - base.b) * (1.0f - blend.b));
    return result;
}

float4 BlendWithMask(float4 base, float4 makeup, float maskAlpha, int blendMode, float intensity, float opacity)
{
    float alpha = maskAlpha * intensity * opacity * makeup.a;
    alpha = saturate(alpha);
    float3 blended;
    if (blendMode == 0) blended = BlendNormal(base.rgb, makeup.rgb);
    else if (blendMode == 1) blended = BlendMultiply(base.rgb, makeup.rgb);
    else if (blendMode == 2) blended = BlendScreen(base.rgb, makeup.rgb);
    else blended = BlendOverlay(base.rgb, makeup.rgb);
    
    float3 result = lerp(base.rgb, blended, alpha);
    return float4(result, base.a);
}
