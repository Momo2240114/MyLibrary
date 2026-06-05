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
    VS_OUT vout;

    float4 worldPos = SkinningPosition(position, boneWeights, boneIndices);
    vout.vertex = mul(worldPos, viewProjection);
    vout.texcoord = texcoord;
    vout.position = worldPos.xyz;
    vout.normal = SkinningVector(normal, boneWeights, boneIndices);
    vout.tangent = SkinningVector(tangent.xyz, boneWeights, boneIndices);

    return vout;
}