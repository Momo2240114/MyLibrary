#include "ParticleCommon.hlsli"

Texture2D g_texture : register(t0);
Texture2D g_maskTexture : register(t1);
Texture2D g_gradientTexture : register(t2);
Texture2D g_shadeGradientTexture : register(t3);
SamplerState g_sampler : register(s0);

static const uint PARTICLE_GRADIENT_SAMPLE_MODE_HORIZONTAL = 0;
static const uint PARTICLE_GRADIENT_DIRECTION_FORWARD = 0;

float GetLuminance(float3 color_)
{
    return dot(color_, float3(0.299f, 0.587f, 0.114f));
}

float2 GetGradientUV(float lifeRate_)
{
    float gradientT_ = saturate(lifeRate_);
    if (gradientDirection != PARTICLE_GRADIENT_DIRECTION_FORWARD)
    {
        gradientT_ = 1.0f - gradientT_;
    }

    gradientT_ = lerp(gradientStartUv, gradientEndUv, gradientT_);
    float fixedUv_ = saturate(gradientFixedUv);
    return gradientSampleMode == PARTICLE_GRADIENT_SAMPLE_MODE_HORIZONTAL
        ? float2(gradientT_, fixedUv_)
        : float2(fixedUv_, gradientT_);
}

float2 GetShadeGradientUV(float lifeRate_)
{
    float shadeT_ = saturate(lifeRate_);
    if (shadeGradientDirection != PARTICLE_GRADIENT_DIRECTION_FORWARD)
    {
        shadeT_ = 1.0f - shadeT_;
    }

    shadeT_ = lerp(shadeGradientStartUv, shadeGradientEndUv, shadeT_);
    float fixedUv_ = saturate(shadeGradientFixedUv);
    return shadeGradientSampleMode == PARTICLE_GRADIENT_SAMPLE_MODE_HORIZONTAL
        ? float2(shadeT_, fixedUv_)
        : float2(fixedUv_, shadeT_);
}

float4 main(GS_OUT input) : SV_TARGET
{
    float4 base_ = g_texture.Sample(g_sampler, input.tex);
    float mask_ = 1.0f;
    if (useMaskTexture != 0)
    {
        mask_ = g_maskTexture.Sample(g_sampler, input.tex).r;
    }

    float4 color_ = input.color;
    if (useGradientTexture != 0)
    {
        float4 gradient_ = g_gradientTexture.Sample(g_sampler, GetGradientUV(input.lifeRate));
        color_.rgb = gradient_.rgb;
        color_.a = input.color.a * base_.a * gradient_.a;
    }
    else
    {
        color_ *= base_;
    }

    if (useShadeGradientTexture != 0)
    {
        float4 shadeColor_ = g_shadeGradientTexture.Sample(g_sampler, GetShadeGradientUV(input.lifeRate));
        color_.rgb *= GetLuminance(shadeColor_.rgb);
    }

    color_.a *= mask_;

    if (useFresnel != 0)
    {
        float2 centeredUV_ = input.localTex * 2.0f - 1.0f;
        float normalZ_ = sqrt(saturate(1.0f - dot(centeredUV_, centeredUV_)));
        float fresnel_ = pow(1.0f - normalZ_, fresnelPower) * fresnelStrength;
        color_.rgb *= 1.0f + fresnel_;
    }

    if (color_.a < 0.01f)
    {
        discard;
    }

    return color_;
}
