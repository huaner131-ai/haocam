#include "makeup_common.hlsl"
float4 PSEyelash(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float thickness = blendParams.y;
    float length = blendParams.z;
    float lashAlpha = mask * saturate(thickness) * saturate(length);
    return BlendWithMask(base, makeupColor, lashAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSMain(PS_INPUT input) : SV_Target { return PSEyelash(input); }
