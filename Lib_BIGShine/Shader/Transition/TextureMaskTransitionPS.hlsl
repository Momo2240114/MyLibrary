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
    float2 maskUV_ = TransformUV(uv_) + timing.y*maskAnimation.yz;
    if (maskAnimation.x > 2.5)
    {
        float count_ = min(maskFrames.z,maskFrames.x*maskFrames.y);
        float frame_ = min(count_-1,floor(progress_*count_));
        maskUV_ = (saturate(maskUV_)*.998+.001+float2(fmod(frame_,maskFrames.x),floor(frame_/maskFrames.x)))/maskFrames.xy;
    }
    float4 mask_ = effectTexture.Sample(noiseSampler,maskUV_);
    int channel_ = (int)maskParameters.w;
    float value_ = panels.w > 0 ? (channel_ < 4 ? mask_[channel_] : dot(mask_.rgb,float3(.2126,.7152,.0722))) : SampleMud(maskUV_);
    value_ = maskParameters.x > 0.5 ? 1 - value_ : value_;
    if (maskAnimation.x > 2.5)
    {
        return Composite((value_-.5)*2,0);
    }
    if (maskAnimation.x > 1.5)
    {
        value_ = saturate(length(point_)/max(length(float2(style.x,1)),.001) + (value_-.5)*maskParameters.z);
    }
    else if (maskAnimation.x > .5)
    {
        value_ = saturate(coordinate_+(value_-.5)*maskParameters.z);
    }
    distance_ = lerp(-softness_ * 2,1 + softness_ * 2,progress_) - value_;
    return Composite(distance_,0);
}
