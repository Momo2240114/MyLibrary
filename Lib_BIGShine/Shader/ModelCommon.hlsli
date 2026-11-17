#include "Scene.hlsli"

// 結合マップの計算結果を、読み込み済みの単独マップがある物性だけ上書きする。
cbuffer CbSeparateMaterialMaps : register(b12)
{
    uint useMetallicMap;
    uint useRoughnessMap;
    uint packedMetallicChannel;
    uint packedRoughnessChannel;
    uint metallicChannel;
    uint roughnessChannel;
    uint2 separateMaterialPadding;
};
Texture2D SeparateMetallicMap : register(t19);
Texture2D SeparateRoughnessMap : register(t20);

// 参照成分を共通のMetallic/Roughnessへ揃え、結合マップだけに従来の係数を適用する。
float2 ApplySeparateMaterialMaps(float4 packedSample_, float2 factors_, SamplerState sampler_, float2 uv_)
{
    float2 metalRough_ = float2(packedSample_[packedMetallicChannel], packedSample_[packedRoughnessChannel]) * factors_;
    if (useMetallicMap != 0)
        metalRough_.x = SeparateMetallicMap.Sample(sampler_, uv_)[metallicChannel];
    if (useRoughnessMap != 0)
        metalRough_.y = SeparateRoughnessMap.Sample(sampler_, uv_)[roughnessChannel];
    return metalRough_;
}

// PBRCommon.hlsli

struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 position : POSITION;
    // ミラーUVでも法線の向きを維持するため、接線の符号をWに残す。
    float4 tangent : TANGENT;
};


// -----------------------------------------------
// シーンカラーと法線を同時に出力する
// -----------------------------------------------
struct PS_OUT
{
    float4 color : SV_TARGET0; // シーンカラー
    float4 normal : SV_TARGET1; // 法線バッファ
};

static const int CascadeShadowCount = 4;

cbuffer CbCascadeShadow : register(b8)
{
    row_major float4x4 cascadeLightViewProjections[CascadeShadowCount];
    float4 cascadeSplits;
    float4 cascadeShadowBias;
    float cascadeShadowAttenuation;
    float cascadeIndirectShadowAttenuation;
    float2 cascadeShadowPadding;
    float4 cascadeShadowSoftness;
};

Texture2D CascadeShadowMap0 : register(t13);
Texture2D CascadeShadowMap1 : register(t14);
Texture2D CascadeShadowMap2 : register(t15);
Texture2D CascadeShadowMap3 : register(t16);
SamplerState ShadowSampler : register(s1);

static const int PoissonDiskSize = 16;
static const float2 PoissonDisk[PoissonDiskSize] =
{
    float2(-0.94201624f, -0.39906216f),
    float2( 0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2( 0.34495938f,  0.29387760f),
    float2(-0.91588581f,  0.45771432f),
    float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f,  0.27676845f),
    float2( 0.97484398f,  0.75648379f),
    float2( 0.44323325f, -0.97511554f),
    float2( 0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f),
    float2( 0.79197514f,  0.19090188f),
    float2(-0.24188840f,  0.99706507f),
    float2(-0.81409955f,  0.91437590f),
    float2( 0.19984126f,  0.78641367f),
    float2( 0.14383161f, -0.14100790f)
};

float RandomShadowRotation(float2 uv_)
{
    return frac(sin(dot(uv_, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float SampleCascadeShadowMap(int index_, float2 uv_)
{
    if (index_ == 0) return CascadeShadowMap0.Sample(ShadowSampler, uv_).r;
    if (index_ == 1) return CascadeShadowMap1.Sample(ShadowSampler, uv_).r;
    if (index_ == 2) return CascadeShadowMap2.Sample(ShadowSampler, uv_).r;
    return CascadeShadowMap3.Sample(ShadowSampler, uv_).r;
}

float2 GetCascadeShadowMapSize(int index_)
{
    uint width_;
    uint height_;
    if (index_ == 0) CascadeShadowMap0.GetDimensions(width_, height_);
    else if (index_ == 1) CascadeShadowMap1.GetDimensions(width_, height_);
    else if (index_ == 2) CascadeShadowMap2.GetDimensions(width_, height_);
    else CascadeShadowMap3.GetDimensions(width_, height_);
    return float2(width_, height_);
}

float CalcCascadeShadowSample(int index_, float3 shadowTexcoord_, float2 uvOffset_)
{
    const float shadowDepth_ = SampleCascadeShadowMap(index_, shadowTexcoord_.xy + uvOffset_);
    return (shadowTexcoord_.z - cascadeShadowBias[index_] <= shadowDepth_) ? 1.0f : 0.0f;
}

float CalcCascadeShadowVisibility(float3 worldPosition)
{
    float shadow = 1.0f;

    [unroll]
    for (int index = 0; index < CascadeShadowCount; ++index)
    {
        float4 lightPosition = mul(float4(worldPosition, 1.0f), cascadeLightViewProjections[index]);
        lightPosition /= lightPosition.w;
        lightPosition.y = -lightPosition.y;
        lightPosition.xy = lightPosition.xy * 0.5f + 0.5f;

        if (lightPosition.z >= 0.0f && lightPosition.z <= 1.0f &&
            lightPosition.x >= 0.0f && lightPosition.x <= 1.0f &&
            lightPosition.y >= 0.0f && lightPosition.y <= 1.0f)
        {
            const float softnessEnabled_ = cascadeShadowSoftness.w;
            const float poissonRadius_ = cascadeShadowSoftness.x;
            const int poissonSampleCount_ = clamp((int)cascadeShadowSoftness.y, 1, PoissonDiskSize);
            if (softnessEnabled_ < 0.5f || poissonRadius_ <= 0.0001f || poissonSampleCount_ <= 1)
            {
                const float lit_ = CalcCascadeShadowSample(index, lightPosition.xyz, float2(0.0f, 0.0f));
                shadow = lit_;
                break;
            }

            const float2 shadowMapSize_ = GetCascadeShadowMapSize(index);
            const float rotationEnabled_ = cascadeShadowSoftness.z;
            const float angle_ = RandomShadowRotation(worldPosition.xz) * 6.28318530718f * rotationEnabled_;
            const float sinAngle_ = sin(angle_);
            const float cosAngle_ = cos(angle_);
            const float2x2 rotation_ = float2x2(cosAngle_, -sinAngle_, sinAngle_, cosAngle_);

            float litSum_ = 0.0f;
            [unroll]
            for (int poissonIndex_ = 0; poissonIndex_ < PoissonDiskSize; ++poissonIndex_)
            {
                if (poissonIndex_ < poissonSampleCount_)
                {
                    const float2 offset_ = mul(rotation_, PoissonDisk[poissonIndex_]) / shadowMapSize_ * poissonRadius_;
                    litSum_ += CalcCascadeShadowSample(index, lightPosition.xyz, offset_);
                }
            }

            shadow = litSum_ / (float)poissonSampleCount_;
            break;
        }
    }

    return shadow;
}

float CalcCascadeShadow(float3 worldPosition)
{
    return lerp(cascadeShadowAttenuation, 1.0f, CalcCascadeShadowVisibility(worldPosition));
}
