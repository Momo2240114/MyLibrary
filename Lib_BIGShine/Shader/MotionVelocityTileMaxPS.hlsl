#include "MotionBlurCommon.hlsli"

Texture2D<float2> VelocityTexture : register(t0);

float4 main(VS_OUT input_) : SV_TARGET
{
    const int2 tilePosition_ = int2(input_.position.xy);
    const int2 tileOrigin_ = tilePosition_ * MOTION_BLUR_TILE_SIZE;
    float2 maximumVelocity_ = 0.0f;
    float maximumLengthSquared_ = 0.0f;

    [loop]
    for (int y_ = 0; y_ < MOTION_BLUR_TILE_SIZE; ++y_)
    {
        [loop]
        for (int x_ = 0; x_ < MOTION_BLUR_TILE_SIZE; ++x_)
        {
            const int2 samplePosition_ = min(tileOrigin_ + int2(x_, y_), screenSize - 1);
            const float2 velocity_ = VelocityTexture.Load(int3(samplePosition_, 0));
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
