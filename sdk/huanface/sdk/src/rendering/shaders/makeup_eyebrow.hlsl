#include "makeup_common.hlsl"
float4 PSEyebrow(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float thickness = blendParams.y;
    float thickAlpha = mask * saturate(thickness);
    return BlendWithMask(base, makeupColor, thickAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSMain(PS_INPUT input) : SV_Target { return PSEyebrow(input); }
