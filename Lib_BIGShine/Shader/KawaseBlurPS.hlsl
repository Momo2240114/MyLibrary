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

// KawaseBlurPS.hlsl
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float2 offset = (g_offset + 0.5) * g_kawaseScale * g_texSize;
    
    // 4点サンプリングに「中心点」を追加して5点にする
    float4 c = tex.Sample(sam, uv); // 中心
    c += tex.Sample(sam, uv + float2(offset.x, offset.y));
    c += tex.Sample(sam, uv + float2(-offset.x, offset.y));
    c += tex.Sample(sam, uv + float2(offset.x, -offset.y));
    c += tex.Sample(sam, uv + float2(-offset.x, -offset.y));

    // 5点の平均をとる
    return float4(c.rgb * 0.2, 1.0);
}

