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
    float axis_ = abs(dot(point_,normalize(geometry.zw)));
    float extent_ = abs(geometry.z) * style.x * max(geometry.x,1-geometry.x) + abs(geometry.w) * max(geometry.y,1-geometry.y);
    float distance_ = axis_ - (1-progress_) * (extent_ + style.y * 2);
    return Composite(distance_,0);
}
