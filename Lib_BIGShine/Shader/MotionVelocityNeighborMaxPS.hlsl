#include "MotionBlurCommon.hlsli"

Texture2D<float2> TileMaxTexture : register(t0);

float4 main(VS_OUT input_) : SV_TARGET
{
    uint width_;
    uint height_;
    TileMaxTexture.GetDimensions(width_, height_);
    const int2 tilePosition_ = int2(input_.position.xy);
    float2 maximumVelocity_ = 0.0f;
    float maximumLengthSquared_ = 0.0f;

    [unroll]
    for (int y_ = -1; y_ <= 1; ++y_)
    {
        [unroll]
        for (int x_ = -1; x_ <= 1; ++x_)
        {
            const int2 samplePosition_ = clamp(tilePosition_ + int2(x_, y_), int2(0, 0), int2(width_, height_) - 1);
            const float2 velocity_ = TileMaxTexture.Load(int3(samplePosition_, 0));
            const float lengthSquared_ = dot(velocity_, velocity_);
            if (lengthSquared_ > maximumLengthSquared_)
            {
                maximumVelocity_ = velocity_;
                maximumLengthSquared_ = lengthSquared_;
            }
        }
    }
    return float4(maximumVelocity_, 0.0f, 1.0f);
}
