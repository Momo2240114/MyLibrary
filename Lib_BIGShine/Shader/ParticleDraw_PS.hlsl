#include "ParticleCommon.hlsli"

Texture2D g_texture : register(t0);
Texture2D g_maskTexture : register(t1);
Texture2D g_gradientTexture : register(t2);
SamplerState g_sampler : register(s0);

float4 main(GS_OUT input) : SV_TARGET
{
    float4 color = input.color;
    color *= g_texture.Sample(g_sampler, input.tex);

    if (useGradientTexture != 0)
    {
        float4 gradient = g_gradientTexture.Sample(g_sampler, float2(saturate(input.lifeRate), 0.5f));
        color *= gradient;
    }

    if (useMaskTexture != 0)
    {
        float mask = g_maskTexture.Sample(g_sampler, input.tex).r;
        color.a *= mask;
    }

    if (color.a < 0.01f) discard;
    return color;
}
