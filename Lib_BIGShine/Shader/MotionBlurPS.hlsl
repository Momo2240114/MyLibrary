Texture2D SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
SamplerState LinearClampSampler : register(s0);

cbuffer CbMotionBlur : register(b0)
{
    int2 screenSize;
    float2 screenPixelSize;
    float4x4 invViewProjection;
    float4x4 previousViewProjection;
    int enabled;
    int sampleCount;
    float strength;
    float maxVelocity;
}

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float2 GetPreviousUV(float2 uv_, float depth_)
{
    float4 clipPosition_ = float4(uv_ * 2.0f - 1.0f, depth_, 1.0f);
    clipPosition_.y *= -1.0f;

    float4 worldPosition_ = mul(clipPosition_, invViewProjection);
    worldPosition_.xyz /= max(worldPosition_.w, 0.00001f);
    worldPosition_.w = 1.0f;

    float4 previousClip_ = mul(worldPosition_, previousViewProjection);
    previousClip_.xyz /= max(previousClip_.w, 0.00001f);

    float2 previousUV_ = previousClip_.xy * float2(0.5f, -0.5f) + 0.5f;
    return previousUV_;
}

float4 main(VS_OUT input_) : SV_TARGET
{
    float2 uv_ = input_.texcoord;
    float4 centerColor_ = SceneTexture.SampleLevel(LinearClampSampler, uv_, 0.0f);

    if (enabled == 0 || strength <= 0.0f)
    {
        return centerColor_;
    }

    float depth_ = DepthTexture.SampleLevel(LinearClampSampler, uv_, 0.0f);
    if (depth_ >= 0.9999f)
    {
        return centerColor_;
    }

    float2 previousUV_ = GetPreviousUV(uv_, depth_);
    float2 velocity_ = (previousUV_ - uv_) * strength;
    float velocityLength_ = length(velocity_);
    if (velocityLength_ > maxVelocity)
    {
        velocity_ *= maxVelocity / max(velocityLength_, 0.00001f);
    }

    int samples_ = clamp(sampleCount, 2, 16);
    float4 color_ = centerColor_;

    [loop]
    for (int sampleIndex_ = 1; sampleIndex_ < samples_; ++sampleIndex_)
    {
        float t_ = (sampleIndex_ / (float)(samples_ - 1)) - 0.5f;
        color_ += SceneTexture.SampleLevel(LinearClampSampler, uv_ + velocity_ * t_, 0.0f);
    }

    return color_ / samples_;
}
