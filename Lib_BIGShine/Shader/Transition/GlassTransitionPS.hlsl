#include "TransitionCommon.hlsli"


float4 main(float4 position_ : SV_POSITION,float2 uv_ : TEXCOORD0,float3 barycentric_ : TEXCOORD1,float background_ : TEXCOORD2) : SV_TARGET
{
    if (timing.x <= 0)
    {
        return 0;
    }
    if (background_ > .5)
    {
        return float4(color.rgb,maskAnimation.w > .5 || timing.x >= 1 ? 1 : timing.x);
    }
    if (maskAnimation.w < .5)
    {
        return 0;
    }
    float progress_ = saturate(Evaluate(timing.x,(int)timing.w));
    float border_ = min(barycentric_.x,min(barycentric_.y,barycentric_.z));
    float edgeWidth_ = max(noise.w*clamp(panels.x,2,6),fwidth(border_)*1.5);
    float crack_ = (1-smoothstep(0,max(.0001,edgeWidth_),border_))*sin(progress_*3.141593);
    // 破片一枚につき元の画面を一回参照し、割れ目の光だけを重ねる。
    uint textureWidth_,textureHeight_;
    sceneSnapshot.GetDimensions(textureWidth_,textureHeight_);
    float2 inset_ = .5/float2(max(textureWidth_,1),max(textureHeight_,1));
    float3 shard_ = sceneSnapshot.SampleLevel(noiseSampler,clamp(uv_,inset_,1-inset_),0).rgb;
    shard_ = lerp(shard_,edgeColor.rgb*(1+noise.z),saturate(crack_*edgeColor.a));
    return float4(lerp(shard_,color.rgb,smoothstep(.8,1,progress_)),1);
}
