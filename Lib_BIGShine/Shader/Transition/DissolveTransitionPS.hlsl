#include "TransitionCommon.hlsli"
float4 main(float4 position_ : SV_POSITION, float2 uv_ : TEXCOORD0) : SV_TARGET
{
    // 終端を明示し、任意Textureでも被覆と透明を保証する。
    if (timing.x <= 0) return float4(0,0,0,0);
    if (timing.x >= 1) return float4(color.rgb,1);
    float progress_ = saturate(Evaluate(timing.x,(int)timing.w));
    float softness_ = max(style.y,0.0001);
    float2 point_ = (uv_ - geometry.xy) * float2(style.x,1);
    float2 direction_ = normalize(geometry.zw);
    float2 normal_ = float2(-direction_.y,direction_.x);
    float extent_ = max(0.001,abs(direction_.x) * style.x + abs(direction_.y));
    float coordinate_ = dot((uv_ - 0.5) * float2(style.x,1),direction_) / extent_ + 0.5;
    float distance_ = 0;
    float flash_ = 0;
        // 指定方向から侵食し、進行に伴って異なる大きさの黒い泥がつながる。
        float field_ = coordinate_ * 0.58 + SampleMud(uv_ * float2(style.x, 1)) * 0.42;
        distance_ = lerp(-0.12, 1.12, progress_) - field_;
    return Composite(distance_,0);
}
