Texture2D sceneTex : register(t0); // 元の描画結果
Texture2D bloomTex : register(t1); // ぼかした結果
SamplerState sam : register(s0);

cbuffer CbBloom : register(b0)
{
    float g_threshold;
    float g_intensity;
    float g_kawaseScale;
    float g_offset;
    float2 g_texSize;
    float g_exposure;
    float g_toneMappingEnabled;
};

// Slot 1: カラーフィルター用
cbuffer CbColorFilter : register(b1)
{
    float g_hueShift;
    float g_saturation;
    float g_brightness;
    float g_contrast;
    float3 g_colorBalance;
    float g_filterIntensity;
};

// RGBをHSV(色相, 彩度, 明度)に変換
float3 RGBtoHSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSVをRGBに変換
float3 HSVtoRGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float3 ToneMapACES(float3 color_)
{
    const float a_ = 2.51f;
    const float b_ = 0.03f;
    const float c_ = 2.43f;
    const float d_ = 0.59f;
    const float e_ = 0.14f;
    return saturate((color_ * (a_ * color_ + b_)) / (color_ * (c_ * color_ + d_) + e_));
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float3 base = sceneTex.Sample(sam, uv).rgb;
    float3 bloom = bloomTex.Sample(sam, uv).rgb;

    // 安全のためNaN/Infガード
    if (any(isnan(bloom)) || any(isinf(bloom)))
        bloom = 0;

    // 1. ブルーム合成
    float3 combined = base + (bloom * g_intensity);

    // Bloom合成後のHDRカラーをLDRへ変換
    if (g_toneMappingEnabled > 0.5f)
        combined = ToneMapACES(max(0.0f, combined) * max(0.0f, g_exposure));

    float3 original = combined; // フィルタ適用前の色を保存

    // --- 2. カラーフィルター処理 ---
    // A. コントラスト (0.5を基準にスケーリング)
    combined = (combined - 0.5f) * g_contrast + 0.5f;

    // B. 白黒化（彩度 0 の処理を HSV 変換前に計算すると高速）
    // 輝度（Luminance）係数を用いて正確なグレースケールを作成
    float gray = dot(combined, float3(0.299, 0.587, 0.114));
    combined = lerp(combined, float3(gray, gray, gray), 1.0f - g_saturation);

    // C. 色相・明度の調整 (彩度は B で処理済みのため hsv.y はそのまま)
    float3 hsv = RGBtoHSV(max(0.0f, combined));
    hsv.x = frac(hsv.x + g_hueShift);
    hsv.z *= g_brightness;
    combined = HSVtoRGB(hsv);

    // D. カラーバランス / オーバーレイ
    // g_colorBalance を「乗算」ではなく「目標の色」として扱う場合
    // 例: combined = lerp(combined, g_targetColor.rgb, g_targetColorIntensity);
    combined *= g_colorBalance;

    // 3. 最終ブレンド
    // g_filterIntensity を 1.0 にすればフィルター完全適用、0.0 なら適用なし
    float3 finalColor = lerp(original, combined, g_filterIntensity);

    return float4(finalColor, 1.0);
}

//float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
//{
//    float3 base = sceneTex.Sample(sam, uv).rgb;
//    float3 bloom = bloomTex.Sample(sam, uv).rgb;

//    // 安全のためNaN/Infガード
//    if (any(isnan(bloom)) || any(isinf(bloom)))
//        bloom = 0;

//    // 単純加算ではなく、強すぎる光を少しマイルドにする
//    // (1.0 + bloom) で割る簡易トーンマップ的な処理を挟むと四角い破片が消えやすいです
//    float3 finalBloom = bloom * g_intensity;
    
//    return float4(base + finalBloom, 1.0);
//}
