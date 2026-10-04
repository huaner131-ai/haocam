/**
 * makeup_blend.hlsl — Phase 9 global tint for sensitivity >0.1
 */
#include "makeup_common.hlsl"

float4 PSBlend(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    float intensity = makeupIntensity * makeupOpacity;
    if (intensity < 0.001f) return base;
    float3 makeup = makeupColor.rgb;
    float3 blended = base.rgb + intensity * 0.15f;
    if (mask > 0.001f) {
        float alpha = saturate(mask * intensity);
        blended = lerp(blended, makeup, alpha);
        blended += mask * intensity * 0.5f;
    }
    return float4(saturate(blended), base.a);
}

float4 PSNormal(PS_INPUT input) : SV_Target { return PSBlend(input); }
float4 PSMultiply(PS_INPUT input) : SV_Target { return PSBlend(input); }
float4 PSScreen(PS_INPUT input) : SV_Target { return PSBlend(input); }
float4 PSOverlay(PS_INPUT input) : SV_Target { return PSBlend(input); }

float4 PSMain(PS_INPUT input) : SV_Target { return PSBlend(input); }
