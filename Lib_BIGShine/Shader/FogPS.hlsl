Texture2D SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
Texture2D FogTexture : register(t2);
SamplerState LinearClampSampler : register(s0);
SamplerState LinearWrapSampler : register(s1);

cbuffer CbFog : register(b0)
{
    float4 fogColor;
    float4 distances;
    float4 controls;
    float4 textureParams;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(VS_OUT input_) : SV_TARGET
{
    float4 sceneColor_ = SceneTexture.Sample(LinearClampSampler, input_.texcoord);
    uint width_;
    uint height_;
    DepthTexture.GetDimensions(width_, height_);
    int2 pixel_ = clamp(int2(input_.position.xy), int2(0, 0), int2(width_ - 1, height_ - 1));
    float depth_ = DepthTexture.Load(int3(pixel_, 0));
    if (depth_ >= 0.99999f) return sceneColor_;

    // 深度バッファの非線形値を視点からの距離へ戻す。
    float viewDistance_ = distances.x * distances.y /
        max(distances.y - depth_ * (distances.y - distances.x), 0.00001f);
    float fogAmount_ = saturate((viewDistance_ - distances.z) / max(distances.w - distances.z, 0.001f)) *
        saturate(controls.x);
    if (textureParams.z > 0.5f)
    {
        float pattern_ = FogTexture.Sample(LinearWrapSampler, input_.texcoord * textureParams.xy).r;
        fogAmount_ *= lerp(1.0f, pattern_, saturate(controls.y));
    }

    sceneColor_.rgb = lerp(sceneColor_.rgb, fogColor.rgb, fogAmount_);
    if (controls.z > 0.0f)
    {
        float line_ = abs(frac((input_.position.x + input_.position.y) * 0.08f * max(controls.w, 0.01f)) - 0.5f);
        float hatch_ = 1.0f - smoothstep(0.04f, 0.10f, line_);
        sceneColor_.rgb *= 1.0f - hatch_ * saturate(controls.z) * fogAmount_ * 0.55f;
    }
    return sceneColor_;
}
