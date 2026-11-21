#include "TransitionCommon.hlsli"

// 終端を明示し、どのパターンでも完全被覆と完全透明を保証する。
float4 main(float4 position_ : SV_POSITION,float2 uv_ : TEXCOORD0) : SV_TARGET
{
    if (timing.x <= 0)
    {
        return float4(0,0,0,0);
    }
    if (timing.x >= 1)
    {
        return float4(color.rgb,1);
    }
    float progress_ = saturate(Evaluate(timing.x,(int)timing.w));
    float2 point_ = (uv_ - geometry.xy) * float2(style.x,1);
    float radius_ = length(point_) / max(length(float2(style.x,1)),0.001);
    float angle_ = atan2(point_.y,point_.x) / 6.283185 + 0.5;
    float field_ = frac(angle_ + radius_ * max(noise.x * 0.35,0.1));
    return Composite(lerp(-0.15,1.15,progress_) - field_,0);
}
