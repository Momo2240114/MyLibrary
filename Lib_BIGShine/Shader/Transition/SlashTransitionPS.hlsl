#include "TransitionCommon.hlsli"

float4 main(float4 position_ : SV_POSITION, float2 uv_ : TEXCOORD0) : SV_TARGET
{
    // 終端を明示し、任意Textureでも被覆と透明を保証する。
    if (timing.x <= 0)
    {
        return float4(0,0,0,0);
    }
    if (timing.x >= 1)
    {
        return float4(color.rgb,1);
    }
    float progress_ = saturate(Evaluate(timing.x,(int)timing.w));
    float softness_ = max(style.y,0.0001);
    float2 point_ = (uv_ - geometry.xy) * float2(style.x,1);
    float2 direction_ = normalize(geometry.zw);
    float2 normal_ = float2(-direction_.y,direction_.x);
    float extent_ = max(0.001,abs(direction_.x) * style.x + abs(direction_.y));
    float coordinate_ = dot((uv_ - 0.5) * float2(style.x,1),direction_) / extent_ + 0.5;
    float distance_ = 0;
    float flash_ = 0;
        // 最初に斬撃の先端を走らせ、その軌跡から両側へ黒い領域を広げる。
        float along_ = dot((uv_ - 0.5) * float2(style.x, 1), direction_) / extent_ + 0.5;
        float perpendicular_ = abs(dot(point_, normal_));
        float normalExtent_ = abs(normal_.x) * style.x * max(geometry.x, 1 - geometry.x) + abs(normal_.y) * max(geometry.y, 1 - geometry.y) + softness_ * 2;
        float spread_ = saturate((progress_ - 0.22) / 0.78);
        distance_ = spread_ * normalExtent_ - perpendicular_;
        float stroke_ = 1 - smoothstep(noise.w * 0.3, noise.w * 1.8, perpendicular_);
        float head_ = 1 - smoothstep(progress_ * 4 - 0.08, progress_ * 4 + 0.02, along_);
        flash_ = stroke_ * head_ * (1 - smoothstep(0.25, 0.55, progress_));
        if (progress_ < 0.22)
    {
        distance_ = -1;
    }
    float2 slashUV_ = float2(along_,dot(point_,normal_) * 8 + 0.5);
    if (panels.w > 0)
    {
        flash_ *= effectTexture.Sample(noiseSampler,TransformUV(slashUV_)).a;
    }
    if (style.w > 0)
    {
        flash_ *= glowTexture.Sample(noiseSampler,TransformUV(slashUV_)).r;
    }
    return Composite(distance_,flash_);
}
