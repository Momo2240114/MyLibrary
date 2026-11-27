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
    [branch]
    if (detail.z > 0.0)
    {
        // 色差の小さい方向を優先し、輪郭を残して筆で置いたような色面にまとめる。
        float2 canvas_ = local_ * float2(1280, 720);
        float brushAngle_ = (Noise(canvas_ / 37.0) - 0.5) * 1.4 - 0.28;
        float2 brushAxis_ = float2(cos(brushAngle_), sin(brushAngle_));
        float2 brush_ = brushAxis_ * abs(sampling.xy) * 5.5;
        float2 crossBrush_ = float2(-brushAxis_.y, brushAxis_.x) * abs(sampling.xy) * 3.5;
        float3 alongA_ = ReadSource(uv_ - brush_).rgb;
        float3 alongB_ = ReadSource(uv_ + brush_).rgb;
        float3 acrossA_ = ReadSource(uv_ - crossBrush_).rgb;
        float3 acrossB_ = ReadSource(uv_ + crossBrush_).rgb;
        float3 alongDelta_ = abs(alongA_ - base_.rgb) + abs(alongB_ - base_.rgb);
        float3 acrossDelta_ = abs(acrossA_ - base_.rgb) + abs(acrossB_ - base_.rgb);
        float3 lumaWeights_ = float3(0.2126, 0.7152, 0.0722);
        float alongWeight_ = exp2(-20.0 * dot(alongDelta_, lumaWeights_));
        float acrossWeight_ = exp2(-20.0 * dot(acrossDelta_, lumaWeights_));
        float3 pigment_ = (base_.rgb + (alongA_ + alongB_) * alongWeight_ +
            (acrossA_ + acrossB_) * acrossWeight_) / (1.0 + 2.0 * (alongWeight_ + acrossWeight_));
        float value_ = dot(pigment_, lumaWeights_);
        pigment_ *= lerp(1.0, round(value_ * 13.0) / max(value_ * 13.0, 0.001), 0.38);
        float fibers_ = sin(canvas_.x * 2.75) * sin(canvas_.y * 3.1) * 0.014;
        float grain_ = (Noise(canvas_ * 0.8) - 0.5) * 0.044;
        float strokes_ = (Noise(canvas_ * float2(0.075, 0.34)) - 0.5) * 0.042;
        base_.rgb = lerp(base_.rgb, max(pigment_ * float3(1.07, 0.99, 0.88) + fibers_ + grain_ + strokes_, 0.0), detail.z);
    }

    [branch]
    if (detail.w > 0.0)
    {
        float value_ = dot(base_.rgb, float3(0.2126, 0.7152, 0.0722));
        float3 crimson_ = float3(1.25, 0.14, 0.24) * pow(max(value_, 0.0), 0.85);
        float highlight_ = smoothstep(0.24, 0.76, value_);
        float3 inkColor_ = lerp(lerp(base_.rgb, crimson_, 0.68), base_.rgb * float3(1.14, 0.88, 0.91), highlight_);
        float2 grainUv_ = float2(local_.x + local_.y * 0.74, local_.y);
        float tornEdge_ = max((abs(local_.x - 0.48) * 2.0 - 0.55) * 1.25, (local_.y - 0.65) * 1.8);
        tornEdge_ += (Noise(grainUv_ * float2(38, 8)) - 0.5) * 0.32;
        tornEdge_ += (Noise(grainUv_ * float2(132, 28)) - 0.5) * 0.11;
        float border_ = smoothstep(0.19, 0.48, tornEdge_);
        float edgeLight_ = smoothstep(0.23, 0.33, tornEdge_) * (1.0 - smoothstep(0.34, 0.50, tornEdge_));
        inkColor_ = lerp(inkColor_, float3(0.012, 0.002, 0.009), border_ * 0.96);
        inkColor_ += float3(0.82, 0.028, 0.095) * edgeLight_ * Noise(grainUv_ * float2(75, 13)) * 0.42;
        base_.rgb = lerp(base_.rgb, inkColor_, detail.w);
    }
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
