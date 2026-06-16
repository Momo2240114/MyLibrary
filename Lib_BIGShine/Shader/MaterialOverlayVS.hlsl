#include "MeshVFX.hlsli"
#include "Skinning.hlsli"

VS_OUT main(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;
    float4 skinnedPosition = SkinningPosition(float4(input.position, 1.0f), input.boneWeights, input.boneIndices);
    float3 skinnedNormal = normalize(SkinningVector(input.normal, input.boneWeights, input.boneIndices));
    float3 skinnedTangent = normalize(SkinningVector(input.tangent.xyz, input.boneWeights, input.boneIndices));

    output.position = mul(skinnedPosition, viewProjection);
    output.worldPosition = skinnedPosition.xyz;
    output.normal = skinnedNormal;
    output.tangent = float4(skinnedTangent, input.tangent.w);
    output.texcoord = input.texcoord;
    return output;
}
