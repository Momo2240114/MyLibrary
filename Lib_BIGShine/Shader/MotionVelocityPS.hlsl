#include "MotionBlurCommon.hlsli"

Texture2D<float> DepthTexture : register(t0);

float4 main(VS_OUT input_) : SV_TARGET
{
    const float depth_ = DepthTexture.Load(int3(input_.position.xy, 0));
    if (enabled == 0 || depth_ >= 0.9999f)
    {
        return 0.0f;
    }

    float4 clipPosition_ = float4(input_.texcoord * 2.0f - 1.0f, depth_, 1.0f);
    clipPosition_.y *= -1.0f;
    float4 worldPosition_ = mul(clipPosition_, invViewProjection);
    if (abs(worldPosition_.w) < 0.00001f)
    {
        return 0.0f;
    }
    worldPosition_ /= worldPosition_.w;

    float4 previousClip_ = mul(float4(worldPosition_.xyz, 1.0f), previousViewProjection);
    if (previousClip_.w <= 0.00001f)
    {
        return 0.0f;
    }
    previousClip_.xy /= previousClip_.w;
    const float2 previousUV_ = previousClip_.xy * float2(0.5f, -0.5f) + 0.5f;
    float2 velocity_ = (input_.texcoord - previousUV_) * strength;
    const float velocityLength_ = length(velocity_);
    if (velocityLength_ > maxVelocity)
    {
        velocity_ *= maxVelocity / max(velocityLength_, 0.00001f);
    }
    return float4(velocity_, 0.0f, 1.0f);
}
