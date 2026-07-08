#include "Skinning.hlsli"

cbuffer CbObjectMotionVelocity : register(b0)
{
    row_major float4x4 currentViewProjection;
    row_major float4x4 previousViewProjection;
    row_major float4x4 currentOwnerWorld;
    row_major float4x4 previousOwnerWorld;
    row_major float4x4 inverseCurrentOwnerWorld;
    float velocityScale;
    float3 padding;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
};

VS_OUT main(
    float4 position : POSITION,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES,
    float2 texcoord : TEXCOORD,
    float3 normal : NORMAL,
    float4 tangent : TANGENT)
{
    VS_OUT output_;
    const float4 currentWorldPosition_ = SkinningPosition(position, boneWeights, boneIndices);
    const float4 ownerLocalPosition_ = mul(currentWorldPosition_, inverseCurrentOwnerWorld);
    const float4 previousWorldPosition_ = mul(ownerLocalPosition_, previousOwnerWorld);

    output_.currentClip = mul(currentWorldPosition_, currentViewProjection);
    output_.previousClip = mul(previousWorldPosition_, previousViewProjection);
    output_.position = output_.currentClip;
    return output_;
}
