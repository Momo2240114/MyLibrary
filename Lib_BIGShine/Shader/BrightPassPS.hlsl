Texture2D tex : register(t0);
SamplerState sam : register(s0);

cbuffer CbBloom : register(b0)
{
    float g_threshold;
    float g_intensity;
    float g_kawaseScale;
    float g_offset;
    float2 g_texSize;
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 color = tex.Sample(sam, uv);
    
    // 1. 安全ガード：NaNや極端な値をここで捨てる
    if (any(isnan(color)) || any(isinf(color)))
        return float4(0, 0, 0, 1);

    float m = max(color.r, max(color.g, color.b));
    // 閾値付近を滑らかに抽出し、明るさの変化でBloomが跳ねるのを抑える。
    float knee_ = max(g_threshold * 0.5f, 0.0001f);
    float soft_ = clamp(m - g_threshold + knee_, 0.0f, 2.0f * knee_);
    soft_ = soft_ * soft_ / (4.0f * knee_);
    float contribution_ = max(m - g_threshold, soft_) / max(m, 0.0001f);
    float3 extracted = color.rgb * contribution_;
    
    // 2. 輝度の上限を制限 (1.0を超えた値を認めつつ、異常値5.0以上はカット)
    // これにより、1ピクセルが巨大な四角になるのを防げます
    extracted = clamp(extracted, 0.0, 5.0);

    return float4(extracted, 1.0);
}
//float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
//{
//    float4 color = tex.Sample(sam, uv);
    
//    // 輝度を計算
//    float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    
//    // ソフトニーの実装：閾値の少し手前から滑らかに抽出し始める
//    // 0.1 などの幅（knee）を持たせることで、光の輪郭が柔らかくなります
//    float knee = 0.1;
//    float soft = brightness - g_threshold + knee;
//    soft = clamp(soft, 0, 2.0 * knee);
//    soft = soft * soft / (4.0 * knee + 0.00001);
    
//    float contribution = max(soft, brightness - g_threshold);
//    contribution /= max(brightness, 0.00001);
    
//    return float4(color.rgb * contribution, 1.0);
//}