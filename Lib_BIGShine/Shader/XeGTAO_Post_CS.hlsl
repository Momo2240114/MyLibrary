Texture2D<float> depthTexture_ : register(t0);
Texture2D<float4> normalTexture_ : register(t1);
RWTexture2D<float> aoTexture_ : register(u0);
SamplerState pointSampler_ : register(s0);

cbuffer CbXeGtao : register(b0)
{
    int2 viewportSize_;
    float2 viewportPixelSize_;
    row_major float4x4 invViewProjection_;
    float radius_;
    float intensity_;
    float power_;
    float sampleRadius_;
    int sampleCount_;
    int frameIndex_;
    int enabled_;
    int showOnly_;
};

float XeGTAO_InterleavedGradientNoise(float2 pixel_)
{
    return frac(52.9829189f * frac(dot(pixel_, float2(0.06711056f, 0.00583715f))));
}

float3 XeGTAO_ReconstructWorldPosition(float2 uv_, float depth_)
{
    float4 clipPosition_ = float4(uv_ * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), depth_, 1.0f);
    float4 worldPosition_ = mul(clipPosition_, invViewProjection_);
    return worldPosition_.xyz / max(worldPosition_.w, 0.00001f);
}

float3 XeGTAO_LoadNormal(float2 uv_)
{
    float3 normal_ = normalTexture_.SampleLevel(pointSampler_, uv_, 0).xyz;
    normal_ = normal_ * 2.0f - 1.0f;

    if (dot(normal_, normal_) < 0.0001f)
    {
        return float3(0.0f, 1.0f, 0.0f);
    }

    return normalize(normal_);
}

float XeGTAO_CalculateOcclusion(uint2 pixel_)
{
    float2 uv_ = (float2(pixel_) + 0.5f) * viewportPixelSize_;
    float centerDepth_ = depthTexture_.SampleLevel(pointSampler_, uv_, 0);

    if (centerDepth_ >= 0.9999f)
    {
        return 1.0f;
    }

    float3 centerPosition_ = XeGTAO_ReconstructWorldPosition(uv_, centerDepth_);
    float3 centerNormal_ = XeGTAO_LoadNormal(uv_);
    float noise_ = XeGTAO_InterleavedGradientNoise(float2(pixel_) + frameIndex_);
    float occlusion_ = 0.0f;
    int validSampleCount_ = 0;

    [loop]
    for (int sampleIndex_ = 0; sampleIndex_ < sampleCount_; ++sampleIndex_)
    {
        float angle_ = (sampleIndex_ + noise_) * 6.28318530718f / max(sampleCount_, 1);
        float stepScale_ = (sampleIndex_ + 1.0f) / max(sampleCount_, 1);
        float2 direction_ = float2(cos(angle_), sin(angle_));
        float2 sampleUv_ = uv_ + direction_ * viewportPixelSize_ * sampleRadius_ * stepScale_;

        if (any(sampleUv_ <= 0.0f) || any(sampleUv_ >= 1.0f))
        {
            continue;
        }

        float sampleDepth_ = depthTexture_.SampleLevel(pointSampler_, sampleUv_, 0);
        if (sampleDepth_ >= 0.9999f)
        {
            continue;
        }

        float3 samplePosition_ = XeGTAO_ReconstructWorldPosition(sampleUv_, sampleDepth_);
        float3 horizonVector_ = samplePosition_ - centerPosition_;
        float distance_ = length(horizonVector_);

        if (distance_ <= 0.0001f)
        {
            continue;
        }

        float normalWeight_ = saturate(dot(centerNormal_, horizonVector_ / distance_) - 0.05f);
        float rangeWeight_ = saturate(1.0f - distance_ / max(radius_, 0.0001f));
        occlusion_ += normalWeight_ * rangeWeight_;
        ++validSampleCount_;
    }

    if (validSampleCount_ == 0)
    {
        return 1.0f;
    }

    float ao_ = 1.0f - saturate(occlusion_ / validSampleCount_);
    return pow(saturate(ao_), max(power_, 0.0001f));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID_ : SV_DispatchThreadID)
{
    if (dispatchThreadID_.x >= (uint)viewportSize_.x || dispatchThreadID_.y >= (uint)viewportSize_.y)
    {
        return;
    }

    uint2 pixel_ = dispatchThreadID_.xy;
    aoTexture_[pixel_] = enabled_ != 0 ? XeGTAO_CalculateOcclusion(pixel_) : 1.0f;
}
