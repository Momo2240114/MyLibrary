#include "MotionBlurCommon.hlsli"

Texture2D SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
Texture2D<float2> VelocityTexture : register(t2);
Texture2D<float2> NeighborMaxTexture : register(t3);
SamplerState LinearClampSampler : register(s0);

float SoftDepthCompare(float depthA_, float depthB_)
{
    return saturate(1.0f - abs(depthA_ - depthB_) / 0.01f);
}

float4 main(VS_OUT input_) : SV_TARGET
{
    const float2 uv_ = input_.texcoord;
    const float4 centerColor_ = SceneTexture.SampleLevel(LinearClampSampler, uv_, 0.0f);
    if (enabled == 0 || strength <= 0.0f)
    {
        return centerColor_;
    }

    const int2 pixelPosition_ = min(int2(input_.position.xy), screenSize - 1);
    const float centerDepth_ = DepthTexture.Load(int3(pixelPosition_, 0));
    const float2 pixelVelocity_ = VelocityTexture.Load(int3(pixelPosition_, 0));
    const float2 neighborVelocity_ = NeighborMaxTexture.SampleLevel(LinearClampSampler, uv_, 0.0f);
    if (max(length(pixelVelocity_), length(neighborVelocity_)) < length(screenPixelSize) * 0.5f)
    {
        return centerColor_;
    }

    float4 color_ = centerColor_;
    float totalWeight_ = 1.0f;
    const int samples_ = clamp(sampleCount, 2, 16);
    [loop]
    for (int sampleIndex_ = 0; sampleIndex_ < samples_; ++sampleIndex_)
    {
        if (sampleIndex_ == samples_ / 2)
        {
            continue;
        }
        const float2 sampleVelocity_ = ((sampleIndex_ & 1) == 0) ? neighborVelocity_ : pixelVelocity_;
        const float offsetScale_ = (sampleIndex_ / (float)(samples_ - 1)) - 0.5f;
        const float2 sampleUV_ = saturate(uv_ + sampleVelocity_ * offsetScale_);
        const int2 samplePosition_ = min(int2(sampleUV_ * screenSize), screenSize - 1);
        const float sampleDepth_ = DepthTexture.Load(int3(samplePosition_, 0));
        const float depthWeight_ = SoftDepthCompare(centerDepth_, sampleDepth_);
        const float distanceWeight_ = saturate(1.0f - abs(offsetScale_));
        const float weight_ = depthWeight_ * distanceWeight_;
        color_ += SceneTexture.SampleLevel(LinearClampSampler, sampleUV_, 0.0f) * weight_;
        totalWeight_ += weight_;
    }
    return color_ / max(totalWeight_, 0.00001f);
}
