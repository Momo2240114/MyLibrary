#include "ModelCommon.hlsli"

cbuffer CbStaticMesh : register(b5)
{
    row_major float4x4 world;
    row_major float4x4 normalTransform;
};

VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    float3 normal : NORMAL,
    float4 tangent : TANGENT
)
{
    VS_OUT output_;

    const float4 worldPosition_ = mul(position, world);
    output_.vertex = mul(worldPosition_, viewProjection);
    output_.texcoord = texcoord;
    output_.position = worldPosition_.xyz;
    output_.normal = mul(float4(normal, 0.0f), normalTransform).xyz;
    output_.tangent = mul(float4(tangent.xyz, 0.0f), normalTransform).xyz;

    return output_;
}
