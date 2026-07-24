cbuffer CbStaticMesh : register(b5)
{
    row_major float4x4 world;
    row_major float4x4 normalTransform;
};

cbuffer CbShadowCaster : register(b8)
{
    row_major float4x4 lightViewProjection;
};

float4 main(
    float4 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 texcoord : TEXCOORD,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES
) : SV_POSITION
{
    const float4 worldPosition_ = mul(position, world);
    return mul(worldPosition_, lightViewProjection);
}
