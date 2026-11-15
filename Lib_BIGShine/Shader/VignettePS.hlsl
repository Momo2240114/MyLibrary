Texture2D SceneTexture : register(t0);
SamplerState LinearClampSampler : register(s0);

cbuffer CbVignette : register(b0)
{
    float2 screenSize;
    float2 center;
    float4 color;
    int enabled;
    float intensity;
    float smoothness;
    float rounded;
    float roundness;
    float opacity;
    float damageOpacity;
    float padding;
}

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(VS_OUT input_) : SV_TARGET
{
    float4 sceneColor_ = SceneTexture.Sample(LinearClampSampler, input_.texcoord);
    if (enabled != 0 && opacity > 0.0f)
    {
        float2 distance_ = abs(input_.texcoord - center) * intensity;
        float aspect_ = screenSize.x / max(screenSize.y, 1.0f);
        distance_.x *= lerp(1.0f, aspect_, saturate(rounded));
        distance_ = pow(saturate(distance_), max(roundness, 0.001f));

        float vignetteFactor_ = pow(saturate(1.0f - dot(distance_, distance_)), max(smoothness, 0.001f));
        float3 vignetteColor_ = lerp(color.rgb, 1.0f.xxx, vignetteFactor_);
        sceneColor_.rgb = lerp(sceneColor_.rgb, sceneColor_.rgb * vignetteColor_, saturate(opacity));
    }
    if (damageOpacity > 0.0f)
    {
        float2 edgeDistance_ = abs(input_.texcoord - 0.5f) * 2.0f;
        float edge_ = smoothstep(0.62f, 1.0f, max(edgeDistance_.x, edgeDistance_.y));
        sceneColor_.rgb = lerp(sceneColor_.rgb, float3(0.65f, 0.12f, 0.16f), edge_ * damageOpacity);
    }
    return sceneColor_;
}
