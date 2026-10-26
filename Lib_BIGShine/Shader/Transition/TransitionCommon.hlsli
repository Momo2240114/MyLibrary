// Sceneと全UIの上へ合成する、共通スクリーンスペースの遷移マスク。
Texture2D noiseTexture : register(t0);
Texture2D effectTexture : register(t1);
Texture2D glowTexture : register(t2);
Texture2D sceneSnapshot : register(t3);
SamplerState noiseSampler : register(s0);
cbuffer TransitionConstants : register(b0)
{
    float4 color;
    float4 edgeColor;
    float4 timing;   // 進行度、実時間、種類、イージング
    float4 geometry; // 中心UV、方向
    float4 noise;    // スケール、速度、発光、境界幅
    float4 style;    // アスペクト比、ぼかし、曲率
    float4 maskUV;
    float4 maskParameters;
    float4 panels;   // パネル数、遅延秒、現在の片道時間
    float4 maskAnimation; // Maskの種類、UV速度、画面コピーの有無
    float4 maskFrames; // 連番マスクの列数、行数、フレーム数
};

// CPU側と同じ補間を用い、パネルごとの開始時刻にも適用する。
float Evaluate(float ratio_, int easing_)
{
    float t_ = saturate(ratio_);
    float inverse_ = 1 - t_;
    float result_ = t_;
    if (easing_ == 1) result_ = t_ * t_ * t_;
    else if (easing_ == 2) result_ = 1 - inverse_ * inverse_ * inverse_;
    else if (easing_ == 3) result_ = t_ < 0.5 ? 4 * t_ * t_ * t_ : 1 - 4 * inverse_ * inverse_ * inverse_;
    else if (easing_ == 4) result_ = t_ >= 1 ? 1 : 1 - exp2(-10 * t_);
    else if (easing_ == 5) result_ = 1 - 2.70158 * inverse_ * inverse_ * inverse_ + 1.70158 * inverse_ * inverse_;
    return result_;
}

// 線形補間したNoise Textureを複数スケールで重ね、泥の大小の凹凸を作る。
float SampleMud(float2 uv_)
{
    float2 base_ = uv_ * noise.x / 128;
    float2 drift_ = timing.y * noise.y * float2(0.011, -0.008);
    return noiseTexture.Sample(noiseSampler, base_ + drift_).r * 0.58 +
        noiseTexture.Sample(noiseSampler, base_ * 2.07 - drift_ * 0.7 + 0.37).r * 0.28 +
        noiseTexture.Sample(noiseSampler, base_ * 4.13 + drift_ * 0.3 + 0.71).r * 0.14;
}


// UV変換は任意マスクとパネル表面Textureで共有する。
float2 TransformUV(float2 uv_)
{
    float2 point_ = (uv_ - 0.5) * maskUV.xy;
    float sine_, cosine_;
    sincos(maskParameters.y, sine_, cosine_);
    return float2(point_.x * cosine_ - point_.y * sine_,point_.x * sine_ + point_.y * cosine_) + 0.5 + maskUV.zw;
}
float4 Composite(float distance_, float flash_)
{
    float coverage_ = smoothstep(-style.y, style.y, distance_);
    float edge_ = (1 - smoothstep(0,max(noise.w,0.0001),abs(distance_))) * sin(timing.x * 3.141593);
    float emission_ = saturate((edge_ + flash_) * noise.z * edgeColor.a);
    float alpha_ = max(coverage_,emission_);
    return float4(lerp(color.rgb,edgeColor.rgb * (1 + noise.z * 0.3),saturate(emission_ / max(alpha_,0.0001))),alpha_);
}
