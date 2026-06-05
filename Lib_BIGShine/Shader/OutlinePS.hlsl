#include "ModelCommon.hlsli"

cbuffer CbOutline : register(b0)
{
    float4 outlineColor;
    float outlineThickness;
    float3 padding;
};

struct OUTLINE_VS_OUT
{
    float4 vertex : SV_POSITION;
    float3 normal : NORMAL;
    float3 position : POSITION;
};

PS_OUT main(OUTLINE_VS_OUT pin)
{
    PS_OUT output;
    output.color = outlineColor;
    output.normal = float4(normalize(pin.normal) * 0.5f + 0.5f, 1.0f);
    output.depth = length(pin.position - cameraPosition.xyz);
    return output;
}