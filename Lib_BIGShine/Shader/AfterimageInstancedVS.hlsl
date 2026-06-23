#include "MeshVFX.hlsli"

struct AfterimageInstanceData
{
    float time;
    float alpha;
    float2 padding;
};

struct AfterimageBoneTransform
{
    row_major float4x4 value;
};

StructuredBuffer<AfterimageBoneTransform> AfterimageBoneTransforms : register(t5);
StructuredBuffer<AfterimageInstanceData> AfterimageInstances : register(t6);

float4 SkinPosition(float4 position_, float4 weights_, uint4 indices_, uint instanceIndex_)
{
    float4 result_ = 0.0f;
    const uint transformOffset_ = instanceIndex_ * 256;
    [unroll]
    for (uint influenceIndex_ = 0; influenceIndex_ < 4; ++influenceIndex_)
    {
        result_ += weights_[influenceIndex_] * mul(
            position_,
            AfterimageBoneTransforms[transformOffset_ + indices_[influenceIndex_]].value);
    }
    return result_;
}

float3 SkinVector(float3 vector_, float4 weights_, uint4 indices_, uint instanceIndex_)
{
    float3 result_ = 0.0f;
    const uint transformOffset_ = instanceIndex_ * 256;
    [unroll]
    for (uint influenceIndex_ = 0; influenceIndex_ < 4; ++influenceIndex_)
    {
        result_ += weights_[influenceIndex_] * mul(
            float4(vector_, 0.0f),
            AfterimageBoneTransforms[transformOffset_ + indices_[influenceIndex_]].value).xyz;
    }
    return result_;
}

VS_OUT main(VS_IN input_, uint instanceIndex_ : SV_InstanceID)
{
    VS_OUT output_ = (VS_OUT)0;
    const float4 position_ = SkinPosition(
        float4(input_.position, 1.0f), input_.boneWeights, input_.boneIndices, instanceIndex_);
    const float3 normal_ = normalize(SkinVector(
        input_.normal, input_.boneWeights, input_.boneIndices, instanceIndex_));
    const float3 tangent_ = normalize(SkinVector(
        input_.tangent.xyz, input_.boneWeights, input_.boneIndices, instanceIndex_));
    const AfterimageInstanceData instance_ = AfterimageInstances[instanceIndex_];

    output_.position = mul(position_, viewProjection);
    output_.worldPosition = position_.xyz;
    output_.normal = normal_;
    output_.tangent = float4(tangent_, input_.tangent.w);
    output_.texcoord = input_.texcoord;
    output_.instanceTime = instance_.time;
    output_.instanceAlpha = instance_.alpha;
    return output_;
}
