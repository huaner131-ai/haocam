// HaoCam fullscreen vertex shader.
// Draws an oversized triangle covering the viewport; UVs are generated from
// the position so no vertex buffer is required.

cbuffer MirrorCB : register(b0)
{
    float cMirrorX; // 1.0 = horizontal mirror (preview), 0.0 = normal
    float cMirrorY;
    float2 cPadding;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    // Fullscreen triangle: (-1,-1), (3,-1), (-1,3)
    float2 pos = float2((vertexId == 1) ? 3.0 : -1.0, (vertexId == 2) ? 3.0 : -1.0);
    output.position = float4(pos, 0.0, 1.0);
    output.uv = float2((pos.x + 1.0) * 0.5, (1.0 - pos.y) * 0.5); // top-left origin

    output.uv.x = lerp(output.uv.x, 1.0 - output.uv.x, cMirrorX);
    output.uv.y = lerp(output.uv.y, 1.0 - output.uv.y, cMirrorY);
    return output;
}
