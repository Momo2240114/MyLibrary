#include "TransitionCommon.hlsli"
#include "ShapeMaskTransition.hlsli"
// 終端を明示し、どのパターンでも完全被覆と完全透明を保証する。
float4 main(float4 position_ : SV_POSITION,float2 uv_ : TEXCOORD0) : SV_TARGET
{
    if (timing.x <= 0) return float4(0,0,0,0);
    if (timing.x >= 1) return float4(color.rgb,1);
    float progress_ = saturate(Evaluate(timing.x,(int)timing.w));
    float2 point_ = (uv_ - geometry.xy) * float2(style.x,1);
    float2 grid_ = float2(panels.x,max(2,round(panels.x / style.x)));
    // 形状マスクでは行数の丸めによる縦横の伸縮を避け、端のセルだけを画面で切り取る。
    if (panels.w > 0.5) grid_ = float2(panels.x,panels.x / max(style.x,0.0001));
    float2 cell_ = floor(uv_ * grid_);
    float stagger_ = frac(dot(cell_,float2(0.618034,0.414214)));
    float delay_ = min(panels.y / max(panels.z,0.001),0.7);
    float local_ = saturate((progress_ - stagger_ * delay_) / max(1-delay_,0.01));
    float2 pointInCell_ = abs(frac(uv_ * grid_) - 0.5) * 2;
    if (panels.w > 0.5)
    {
        // セル内の位置と遅延は既存演出と共有し、画像の閾値ではなく大きさを進める。
        float2 shapePoint_ = (frac(uv_ * grid_) - 0.5) * 2;
        float4 result_ = Composite(SampleShapeDistance(shapePoint_,local_ * maskFrames.z),0);
        result_.a *= smoothstep(0,0.01,local_);
        return CompleteShapeCover(result_,progress_);
    }
    return Composite(local_ * 1.1 - max(pointInCell_.x,pointInCell_.y),0);
}
