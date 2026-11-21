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
        float row_ = min(floor(uv_.y * panels.x), panels.x - 1);
        float delayRatio_ = min(panels.y / max(panels.z, 0.001), 0.8 / max(panels.x - 1, 1));
        float local_ = (timing.x - row_ * delayRatio_) / max(1 - delayRatio_ * (panels.x - 1), 0.01);
        float panelProgress_ = saturate(Evaluate(local_, (int)timing.w));
        float x_ = fmod(row_, 2) < 1 ? uv_.x : 1 - uv_.x;
        distance_ = panelProgress_ * (1 + softness_ * 4) - x_ - softness_ * 2;
    float4 result_ = Composite(distance_,0);
    if (panels.w > 0)
    {
        result_.rgb += effectTexture.Sample(noiseSampler,TransformUV(uv_)).rgb * smoothstep(-style.y,style.y,distance_) * maskParameters.z;
    }
    return result_;
}
