Texture2D SceneTexture : register(t0);
SamplerState LinearClampSampler : register(s0);

cbuffer CbFxaa : register(b0)
{
    float2 inverseScreenSize;
    float edgeThreshold;
    float edgeThresholdMin;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float Luma(float3 color_)
{
    return dot(color_, float3(0.299f, 0.587f, 0.114f));
}

float4 main(VS_OUT input_) : SV_TARGET
{
    float2 uv_ = input_.texcoord;
    float4 center_ = SceneTexture.Sample(LinearClampSampler, uv_);
    float3 nw_ = SceneTexture.Sample(LinearClampSampler, uv_ + inverseScreenSize * float2(-1.0f, -1.0f)).rgb;
    float3 ne_ = SceneTexture.Sample(LinearClampSampler, uv_ + inverseScreenSize * float2(1.0f, -1.0f)).rgb;
    float3 sw_ = SceneTexture.Sample(LinearClampSampler, uv_ + inverseScreenSize * float2(-1.0f, 1.0f)).rgb;
    float3 se_ = SceneTexture.Sample(LinearClampSampler, uv_ + inverseScreenSize * float2(1.0f, 1.0f)).rgb;
    float lumaCenter_ = Luma(center_.rgb);
    float lumaNw_ = Luma(nw_);
    float lumaNe_ = Luma(ne_);
    float lumaSw_ = Luma(sw_);
    float lumaSe_ = Luma(se_);
    float lumaMin_ = min(lumaCenter_, min(min(lumaNw_, lumaNe_), min(lumaSw_, lumaSe_)));
    float lumaMax_ = max(lumaCenter_, max(max(lumaNw_, lumaNe_), max(lumaSw_, lumaSe_)));
    if (lumaMax_ - lumaMin_ < max(edgeThresholdMin, lumaMax_ * edgeThreshold))
    {
        return center_;
    }

    float2 direction_ = float2(
        -((lumaNw_ + lumaNe_) - (lumaSw_ + lumaSe_)),
        (lumaNw_ + lumaSw_) - (lumaNe_ + lumaSe_));
    float reduce_ = max((lumaNw_ + lumaNe_ + lumaSw_ + lumaSe_) * 0.03125f, 0.0078125f);
    direction_ = clamp(direction_ / (min(abs(direction_.x), abs(direction_.y)) + reduce_),
        float2(-8.0f, -8.0f), float2(8.0f, 8.0f)) * inverseScreenSize;

    float3 colorA_ = 0.5f * (
        SceneTexture.Sample(LinearClampSampler, uv_ + direction_ * (-1.0f / 6.0f)).rgb +
        SceneTexture.Sample(LinearClampSampler, uv_ + direction_ * (1.0f / 6.0f)).rgb);
    float3 colorB_ = colorA_ * 0.5f + 0.25f * (
        SceneTexture.Sample(LinearClampSampler, uv_ + direction_ * -0.5f).rgb +
        SceneTexture.Sample(LinearClampSampler, uv_ + direction_ * 0.5f).rgb);
    float lumaB_ = Luma(colorB_);
    return float4(lumaB_ < lumaMin_ || lumaB_ > lumaMax_ ? colorA_ : colorB_, center_.a);
}
