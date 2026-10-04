#include "makeup_common.hlsl"
float4 PSPupil(PS_INPUT input) : SV_Target
{
    float4 base = inputTexture.Sample(samplerLinear, input.uv);
    float mask = maskTexture.Sample(samplerLinear, input.uv).r;
    int blendMode = (int)blendParams.x;
    float irisEnhance = blendParams.z;
    float scale = blendParams.w;
    float pupilAlpha = mask * saturate(irisEnhance) * saturate(scale);
    return BlendWithMask(base, makeupColor, pupilAlpha, blendMode, makeupIntensity, makeupOpacity);
}
float4 PSMain(PS_INPUT input) : SV_Target { return PSPupil(input); }
