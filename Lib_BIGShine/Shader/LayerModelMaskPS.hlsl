Texture2D BaseMap : register(t0);
Texture2D EmissiveMap : register(t1);
SamplerState BaseSampler : register(s0);

// CbGBufferMaterial と同じ位置の色・UV設定を参照する。
cbuffer CbLayerModelMask : register(b0)
{
    float4 baseColor : packoffset(c0);
    float3 emissiveColor : packoffset(c1);
    float2 uvScale : packoffset(c3.x);
    float2 uvOffset : packoffset(c3.z);
    float uvRotation : packoffset(c4.x);
};

cbuffer CbLayerModel : register(b1)
{
    row_major float4x4 previousObjectProjection;
    float2 pixelSize;
    uint mode;
    uint hasObjectHistory;
};

float4 main(float4 position_ : SV_POSITION, float2 uv_ : TEXCOORD, float3 worldPosition_ : POSITION) : SV_TARGET
{
    const float2 centered_ = (uv_ - 0.5f) * uvScale;
    const float sine_ = sin(uvRotation);
    const float cosine_ = cos(uvRotation);
    const float2 materialUv_ = float2(
        centered_.x * cosine_ - centered_.y * sine_,
        centered_.x * sine_ + centered_.y * cosine_) + 0.5f + uvOffset;
    const float4 base_ = BaseMap.Sample(BaseSampler, materialUv_) * baseColor;
    clip(base_.a - 0.1f);
    const uint BLOOM_COLOR = 1u;
    const uint MODEL_VELOCITY = 2u;
    const uint SILHOUETTE_EXCLUSION = 3u;
    if (mode == SILHOUETTE_EXCLUSION)
    {
        return 1.0f;
    }
    if (mode == BLOOM_COLOR)
    {
        return float4(base_.rgb + EmissiveMap.Sample(BaseSampler, materialUv_).rgb * emissiveColor, base_.a);
    }
    if (mode == MODEL_VELOCITY && hasObjectHistory != 0)
    {
        const float4 previousClip_ = mul(float4(worldPosition_, 1.0f), previousObjectProjection);
        if (previousClip_.w > 0.00001f)
        {
            const float2 previousUV_ = previousClip_.xy / previousClip_.w * float2(0.5f, -0.5f) + 0.5f;
            return float4(position_.xy * pixelSize - previousUV_, 0.0f, 1.0f);
        }
    }
    return 0.0f;
}

