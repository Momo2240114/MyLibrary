#include "Skinning.hlsli"

#include "Scene.hlsli"
#include "ModelCommon.hlsli" 


// GltfModelCommponennt::InputElementDescs の定義順に合わせる
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

    // スキニング：ボーンによる変形
    // float3のpositionをfloat4に変換して渡す必要がある
    float4 worldPos = SkinningPosition(position, boneWeights, boneIndices);
    
    // 座標変換
    vout.vertex = mul(worldPos, viewProjection);
    
    // データ受け渡し
    vout.texcoord = texcoord;
    vout.position = worldPos.xyz;
    
    // 法線と接線のスキニング
    vout.normal = SkinningVector(normal, boneWeights, boneIndices);
    vout.tangent = SkinningVector(tangent.xyz, boneWeights, boneIndices);
    
    return vout;
}