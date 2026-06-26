Texture2D SceneTexture : register(t0);
Texture2D DistortionTexture : register(t1);
SamplerState LinearClampSampler : register(s0);

cbuffer CbPostDistortion : register(b0)
{
    float2 screenSize;
    float2 padding;
}

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(VS_OUT input_) : SV_TARGET
{
    float4 distortion_ = DistortionTexture.SampleLevel(LinearClampSampler, input_.texcoord, 0.0f);
    float opacity_ = saturate(distortion_.a);
    float2 offset_ = distortion_.rg / max(screenSize, 1.0f.xx);

    float4 sceneColor_ = SceneTexture.SampleLevel(LinearClampSampler, input_.texcoord, 0.0f);
    if (opacity_ <= 0.0001f || dot(abs(offset_), 1.0f.xx) <= 0.000001f)
    {
        return sceneColor_;
    }

    float4 distortedColor_ = SceneTexture.SampleLevel(
        LinearClampSampler,
        saturate(input_.texcoord + offset_),
        0.0f);
    return lerp(sceneColor_, distortedColor_, opacity_);
}
