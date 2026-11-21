#include "Skinning.hlsli"
#include "Scene.hlsli"

cbuffer CbOutline : register(b0)
{
    float4 outlineColor;
    float outlineThickness;
    float depthScale;
    float maxDistanceScale;
    float padding;
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

    float thickness_ = outlineThickness;
    if (depthScale > 0.0f)
    {
        // 透視投影では押し出し前のclip.wが視点空間の深度になる。
        const float4 clipWColumn_ = float4(
            viewProjection._14, viewProjection._24, viewProjection._34, viewProjection._44);
        const float viewDepth_ = dot(worldPos, clipWColumn_);
        thickness_ *= clamp(viewDepth_ * depthScale, 0.0f, maxDistanceScale);
    }

    worldPos.xyz += worldNormal * thickness_;

    vout.vertex = mul(worldPos, viewProjection);
    vout.normal = worldNormal;
    vout.position = worldPos.xyz;

    return vout;
}