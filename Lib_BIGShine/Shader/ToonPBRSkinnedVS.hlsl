#include "Skinning.hlsli"
#include "ModelCommon.hlsli"

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

    const float4 worldPosition_ = SkinningPosition(position, boneWeights, boneIndices);
    output_.vertex = mul(worldPosition_, viewProjection);
    output_.texcoord = texcoord;
    output_.position = worldPosition_.xyz;
    output_.normal = SkinningVector(normal, boneWeights, boneIndices);
    output_.tangent = SkinningVector(tangent.xyz, boneWeights, boneIndices);

    return output_;
}
