Texture2D sceneTexture_ : register(t0);
Texture2D<float> aoTexture_ : register(t1);
SamplerState linearSampler_ : register(s0);

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

float4 main(float4 position_ : SV_POSITION, float2 uv_ : TEXCOORD0) : SV_TARGET
{
    float3 sceneColor_ = sceneTexture_.Sample(linearSampler_, uv_).rgb;

    if (enabled_ == 0)
    {
        return float4(sceneColor_, 1.0f);
    }

    float ao_ = saturate(aoTexture_.Sample(linearSampler_, uv_));
    float aoFactor_ = lerp(1.0f, ao_, saturate(intensity_));

    if (showOnly_ != 0)
    {
        return float4(ao_.xxx, 1.0f);
    }

    return float4(sceneColor_ * aoFactor_, 1.0f);
}
