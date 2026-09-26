// HaoCam color pass (stage 2 "Color Conversion" + stage 8 "Color/LUT").
//
// Converts NV12 (BT.709, limited range - standard for UVC cameras) to
// linear-monitor RGB and applies the Phase-1 color adjustments
// (brightness / contrast / saturation). A real LUT stage replaces the
// saturation path in a later phase.

Texture2D    yTexture  : register(t0); // R8_UNORM
Texture2D    uvTexture : register(t1); // R8G8_UNORM
SamplerState linearSampler : register(s0);

cbuffer ColorParams : register(b1)
{
    float cBrightness;  // [-1, 1]
    float cContrast;    // [-1, 1]
    float cSaturation;  // [-1, 1]
    float cPadding0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float3 nv12ToRgb(float2 uv)
{
    float y = yTexture.Sample(linearSampler, uv).r;
    float2 uvSampled = uvTexture.Sample(linearSampler, uv).rg;

    // Limited-range BT.709 (16-235 Y, 16-240 UV) to full-range RGB.
    y = (y - 16.0 / 255.0) * (255.0 / 219.0);
    float u = (uvSampled.x - 0.5) * (255.0 / 224.0);
    float v = (uvSampled.y - 0.5) * (255.0 / 224.0);

    float3 rgb;
    rgb.r = y + 1.5748 * v;
    rgb.g = y - 0.1873 * u - 0.4681 * v;
    rgb.b = y + 1.8556 * u;
    return saturate(rgb);
}

float4 main(VSOutput input) : SV_Target
{
    float3 rgb = nv12ToRgb(input.uv);

    // Brightness (additive), contrast (pivot 0.5), saturation (luma pivot).
    rgb += cBrightness;
    rgb = (rgb - 0.5) * (1.0 + cContrast) + 0.5;
    float luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
    rgb = lerp(float3(luma, luma, luma), rgb, 1.0 + cSaturation);

    return float4(saturate(rgb), 1.0);
}
