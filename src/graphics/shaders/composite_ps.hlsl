// HaoCam final composite pass (stage 9).
// Draws the processed texture into the final output texture. Aspect-ratio
// fit is handled on the CPU by adjusting the viewport; outside the fitted
// area the render target is cleared to black.

Texture2D    sourceTexture : register(t0);
SamplerState pointSampler : register(s0);

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(VSOutput input) : SV_Target
{
    return float4(sourceTexture.Sample(pointSampler, input.uv).rgb, 1.0);
}
