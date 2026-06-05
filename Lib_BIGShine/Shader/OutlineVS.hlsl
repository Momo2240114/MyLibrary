#include "Skinning.hlsli"
#include "Scene.hlsli"

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

OUTLINE_VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    float3 normal : NORMAL,
    float4 tangent : TANGENT
)
{
    OUTLINE_VS_OUT vout;

    float4 worldPos = SkinningPosition(position, boneWeights, boneIndices);
    float3 worldNormal = normalize(SkinningVector(normal, boneWeights, boneIndices));

    worldPos.xyz += worldNormal * outlineThickness;

    vout.vertex = mul(worldPos, viewProjection);
    vout.normal = worldNormal;
    vout.position = worldPos.xyz;

    return vout;
}