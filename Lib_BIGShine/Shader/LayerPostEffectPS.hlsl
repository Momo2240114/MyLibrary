Texture2D<float> LayerDepth : register(t0);
Texture2D<float> SceneDepth : register(t1);
Texture2D<float4> LayerInput : register(t2);
Texture2D<float4> MotionSource : register(t3);
SamplerState LinearClampSampler : register(s0);
SamplerState PointClampSampler : register(s1);

cbuffer CbLayerPostEffect : register(b0)
{
    float4 effectColor;
    uint effectType;
    int sampleCount;
    float strength;
    float maxVelocity;
    row_major float4x4 inverseViewProjection;
    row_major float4x4 previousViewProjection;
    uint useCameraVelocity;
    float3 padding;
};

bool IsVisibleLayerPixel(int2 pixel_)
{
    const float targetDepth_ = LayerDepth.Load(int3(pixel_, 0));
    const float sceneDepth_ = SceneDepth.Load(int3(pixel_, 0));
    const float DEPTH_TOLERANCE = 2.0f / 16777215.0f;
    return targetDepth_ < 1.0f && targetDepth_ <= sceneDepth_ + DEPTH_TOLERANCE;
}

float4 main(float4 position_ : SV_POSITION, float2 uv_ : TEXCOORD0) : SV_TARGET
{
    const uint BLOOM = 2u;
    const uint MOTION_BLUR = 3u;
    if (effectType == BLOOM)
    {
        return float4(LayerInput.SampleLevel(LinearClampSampler, uv_, 0).rgb * strength, 1.0f);
    }
    uint width_, height_;
    LayerDepth.GetDimensions(width_, height_);
    const int2 size_ = int2(width_, height_);
    const int2 pixel_ = min(int2(saturate(uv_) * size_), size_ - 1);
    const float targetDepth_ = LayerDepth.Load(int3(pixel_, 0));
    if (targetDepth_ >= 1.0f)
    {
        discard;
    }

    const bool visible_ = IsVisibleLayerPixel(pixel_);
    if (effectType == MOTION_BLUR)
    {
        if (!visible_)
        {
            discard;
        }

        float2 velocity_ = 0.0f;
        if (useCameraVelocity != 0)
        {
            const float4 world_ = mul(float4(uv_ * float2(2, -2) + float2(-1, 1), targetDepth_, 1),
                inverseViewProjection);
            const float4 previousClip_ = mul(world_, previousViewProjection);
            if (previousClip_.w > 0.00001f)
            {
                const float2 previousUV_ = previousClip_.xy / previousClip_.w * float2(0.5f, -0.5f) + 0.5f;
                velocity_ = uv_ - previousUV_;
            }
        }
        else
        {
            velocity_ = LayerInput.Load(int3(pixel_, 0)).xy;
        }
        velocity_ *= strength;
        const float speed_ = length(velocity_);
        if (speed_ < 0.5f / max(width_, height_))
        {
            discard;
        }
        velocity_ *= min(1.0f, maxVelocity / speed_);

        float3 color_ = MotionSource.SampleLevel(PointClampSampler, uv_, 0).rgb;
        float weight_ = 1.0f;
        const int samples_ = clamp(sampleCount, 2, 16);
        [loop]
        for (int index_ = 1; index_ < samples_; ++index_)
        {
            const float2 sampleUV_ = saturate(uv_ - velocity_ * (index_ / float(samples_ - 1)));
            const int2 samplePixel_ = min(int2(sampleUV_ * size_), size_ - 1);
            // 遮蔽物や別レイヤーの色をブラーに混ぜない。
            if (IsVisibleLayerPixel(samplePixel_))
            {
                color_ += MotionSource.SampleLevel(PointClampSampler, sampleUV_, 0).rgb;
                weight_ += 1.0f;
            }
        }
        return float4(color_ / weight_, 1.0f);
    }
    static const uint OCCLUDED_SILHOUETTE = 0u;
    const bool silhouette_ = effectType == OCCLUDED_SILHOUETTE;
    const bool occluded_ = !visible_;
    if (occluded_ != silhouette_ || (silhouette_ && LayerInput.Load(int3(pixel_, 0)).r > 0.5f))
    {
        discard;
    }
    return effectColor;
}

