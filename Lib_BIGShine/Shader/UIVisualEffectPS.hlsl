#include "sprite.hlsli"
Texture2D sourceTexture : register(t0);
Texture2D maskTexture : register(t1);
SamplerState sourceSampler : register(s0);

// CPU側UIEffectConstantsと同じ順序。周辺サンプルの幅は表示ピクセル単位。
cbuffer UIEffects : register(b1)
{
    float4 glowColor, outlineColor, shadowColor, sweepColor, gradientTop, gradientBottom;
    float4 tone, style, animation, sweep, sampling, shadow, uvRect, detail, burnColor;
    float4 maskUV, maskOptions;
};

float Hash(float2 point_)
{
    return frac(sin(dot(point_,float2(127.1,311.7)))*43758.5453);
}

float Noise(float2 point_)
{
    float2 cell_ = floor(point_), fraction_ = frac(point_);
    fraction_ = fraction_*fraction_*(3-2*fraction_);
    return lerp(lerp(Hash(cell_),Hash(cell_+float2(1,0)),fraction_.x),lerp(Hash(cell_+float2(0,1)),Hash(cell_+1),fraction_.x),fraction_.y);
}

float4 ReadSource(float2 uv_)
{
    if (any(uv_ < uvRect.xy) || any(uv_ > uvRect.zw))
    {
        return 0;
    }
    float2 local_ = (uv_-uvRect.xy)/max(uvRect.zw-uvRect.xy,0.00001);
    if (any(abs(detail.xy) > 0))
    {
        local_ = frac(local_+animation.x*detail.xy);
    }
    float2 pixelStep_ = abs(sampling.xy)*tone.w;
    uv_ = uvRect.xy+local_*(uvRect.zw-uvRect.xy);
    if (tone.w > 1.01)
    {
        uv_ = uvRect.xy+(floor((uv_-uvRect.xy)/max(pixelStep_,0.000001))+0.5)*pixelStep_;
    }
    uint width_,height_;
    sourceTexture.GetDimensions(width_,height_);
    float2 halfTexel_ = 0.5/float2(max(width_,1),max(height_,1));
    float4 value_ = sourceTexture.Sample(sourceSampler,clamp(uv_,uvRect.xy+halfTexel_,uvRect.zw-halfTexel_));
    if (style.w > 0)
    {
        float2 maskPoint_ = local_*maskUV.xy+maskUV.zw;
        float noise_ = maskOptions.x > 0 ? maskTexture.Sample(sourceSampler,frac(maskPoint_)).r : Noise(local_*sweep.w);
        if (maskOptions.y > 0.5)
        {
            noise_ = 1-noise_;
        }
        value_.a *= style.w >= 1 ? 0 : smoothstep(style.w-sweep.z,style.w,noise_);
        value_.rgb += burnColor.rgb*burnColor.a*(1-smoothstep(style.w,style.w+sweep.z,noise_));
    }
    return value_;
}

float AlphaAt(float2 uv_)
{
    return ReadSource(uv_).a;
}

float Ring(float2 uv_,float radius_)
{
    float value_ = 0;
    [unroll] for (int index_ = 0; index_ < 8; ++index_)
    {
        float angle_ = index_*0.785398163;
        value_ += AlphaAt(uv_+float2(cos(angle_),sin(angle_))*abs(sampling.xy)*radius_);
    }
    return value_*0.125;
}

void Over(inout float4 premultiplied_,float3 rgb_,float alpha_)
{
    alpha_ = saturate(alpha_);
    premultiplied_.rgb = rgb_*alpha_+premultiplied_.rgb*(1-alpha_);
    premultiplied_.a = alpha_+premultiplied_.a*(1-alpha_);
}

float4 main(VS_OUT input_) : SV_TARGET
{
    float2 uv_ = input_.texcoord;
    float4 base_ = ReadSource(uv_);
    if (style.z > 0)
    {
        float4 accumulation_ = float4(base_.rgb*base_.a,base_.a)*0.4;
        [unroll] for (int index_ = 0; index_ < 4; ++index_)
        {
            float angle_ = index_*1.570796327;
            float4 tap_ = ReadSource(uv_+float2(cos(angle_),sin(angle_))*abs(sampling.xy)*style.z);
            accumulation_ += float4(tap_.rgb*tap_.a,tap_.a)*0.15;
        }
        base_ = float4(accumulation_.rgb/max(accumulation_.a,0.00001),accumulation_.a);
    }
    float2 local_ = (uv_-uvRect.xy)/max(uvRect.zw-uvRect.xy,0.00001);
    base_.rgb *= input_.color.rgb;
    base_.rgb *= lerp(1,lerp(gradientTop.rgb,gradientBottom.rgb,saturate(local_.y)),sampling.w);
    float luminance_ = dot(base_.rgb,float3(0.2126,0.7152,0.0722));
    base_.rgb = lerp(base_.rgb,luminance_.xxx,tone.x);
    float3 sepia_ = float3(dot(base_.rgb,float3(.393,.769,.189)),dot(base_.rgb,float3(.349,.686,.168)),dot(base_.rgb,float3(.272,.534,.131)));
    base_.rgb = lerp(base_.rgb,sepia_,tone.y);
    base_.rgb = lerp(base_.rgb,1-saturate(base_.rgb),tone.z);
    base_.rgb *= 1+animation.y*(0.5+0.5*sin(animation.x*animation.z*6.283185307));
    float2 direction_ = float2(cos(sweep.x),sin(sweep.x));
    float coordinate_ = dot(local_-.5,direction_);
    float highlight_ = 1-smoothstep(0,max(.001,sweep.y),abs(coordinate_-(frac(animation.x*animation.w)*2-1)));
    base_.rgb += sweepColor.rgb*sweepColor.a*highlight_;
    float4 result_ = 0;
    if (shadowColor.a > 0)
    {
        Over(result_,shadowColor.rgb,lerp(AlphaAt(uv_-shadow.xy*abs(sampling.xy)),Ring(uv_-shadow.xy*abs(sampling.xy),sampling.z),step(.01,sampling.z))*shadowColor.a);
    }
    if (glowColor.a > 0)
    {
        Over(result_,glowColor.rgb,Ring(uv_,style.x)*glowColor.a);
    }
    if (style.y > 0)
    {
        Over(result_,outlineColor.rgb,saturate(Ring(uv_,style.y)*4)*outlineColor.a);
    }
    Over(result_,base_.rgb,base_.a);
    result_.rgb /= max(result_.a,0.00001);
    result_.a *= input_.color.a;
    if (result_.a <= .00001)
    {
        discard;
    }
    return result_;
}
