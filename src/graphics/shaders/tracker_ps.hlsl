// HaoCam tracking readback pass.
// Downscales the processed BGRA frame to the tracking working resolution and
// re-orders channels so the staged BGRA8 memory reads as RGBA8 bytes
// (MediaPipe expects SRGBA byte order: R,G,B,A).
//
// BGRA8 render-target byte layout in memory is [B][G][R][A]. Writing
// float4(c.b, c.g, c.r, 1) stores sampled.R in the B channel (byte 0),
// sampled.G in G (byte 1), sampled.B in the R channel (byte 2):
// memory becomes [R][G][B][A] = SRGBA byte order.

Texture2D    sourceTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(VSOutput input) : SV_Target
{
    float3 c = sourceTexture.Sample(linearSampler, input.uv).rgb;
    return float4(c.bgr, 1.0);
}
