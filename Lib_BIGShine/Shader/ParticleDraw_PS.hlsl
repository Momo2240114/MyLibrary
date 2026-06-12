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

    if (useFresnel != 0)
    {
        float2 centeredUV_ = input.localTex * 2.0f - 1.0f;
        float normalZ_ = sqrt(saturate(1.0f - dot(centeredUV_, centeredUV_)));
        float fresnel_ = pow(1.0f - normalZ_, fresnelPower) * fresnelStrength;
        color.rgb *= 1.0f + fresnel_;
    }

    if (color.a < 0.01f) discard;
    return color;
}
