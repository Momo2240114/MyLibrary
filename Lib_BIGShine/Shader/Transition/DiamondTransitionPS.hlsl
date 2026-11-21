#include "TransitionCommon.hlsli"
#include "ShapeMaskTransition.hlsli"

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
    float angle_ = atan2(geometry.w,geometry.z);
    float2 rotated_ = float2(point_.x * cos(angle_) + point_.y * sin(angle_),-point_.x * sin(angle_) + point_.y * cos(angle_));
    float extent_ = style.x + 1;
    if (panels.w > 0.5)
    {
        // Textureの同じ輪郭を縮め、菱形以外のシルエットでも中央開口として使う。
        float radius_ = (1-progress_) * extent_ * maskFrames.z;
        float4 result_ = Composite(-SampleShapeDistance(rotated_,radius_),0);
        // 中央が空洞の画像や偏った配置でも、開始フレームに突然被覆しない。
        result_.a *= smoothstep(0,maskFrames.x,progress_);
        return CompleteShapeCover(result_,progress_);
    }
    float distance_ = abs(rotated_.x) + abs(rotated_.y) - (1 - progress_) * (extent_ + style.y * 2);
    return Composite(distance_,0);
}
