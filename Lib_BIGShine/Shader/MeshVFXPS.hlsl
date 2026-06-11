#include "MeshVFX.hlsli"

Texture2D BaseTexture : register(t0);
Texture2D MaskTexture : register(t1);
Texture2D NoiseTexture : register(t2);

SamplerState LinearSampler : register(s0);

float2 RotateUV(float2 uv, float angle)
{
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    float2 centeredUV = uv - uvRotationCenter;
    return float2(
        centeredUV.x * cosine - centeredUV.y * sine,
        centeredUV.x * sine + centeredUV.y * cosine) + uvRotationCenter;
}

float4 main(VS_OUT input) : SV_TARGET
{
    float2 baseUV = RotateUV(input.texcoord, baseUVRotation) + baseUVSpeed * time;
    float2 maskUV = RotateUV(input.texcoord, maskUVRotation) + maskUVSpeed * time;
    float2 noiseUV = RotateUV(input.texcoord, noiseUVRotation) + noiseUVSpeed * time;

    if (useNoise != 0)
    {
        float2 noise = NoiseTexture.Sample(LinearSampler, noiseUV).rg * 2.0f - 1.0f;

        baseUV += noise * noiseStrength;
        maskUV += noise * noiseStrength;
    }

    float4 baseColor = BaseTexture.Sample(LinearSampler, baseUV);
    baseColor *= vfxColor;

    float mask = 1.0f;

    if (useMask != 0)
    {
        mask = MaskTexture.Sample(LinearSampler, maskUV).r;
    }

    if (mask < dissolveThreshold)
    {
        discard;
    }

    float fresnel = 1.0f;

    if (useFresnel != 0)
    {
        float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
        float3 normal = normalize(input.normal);

        float f = 1.0f - saturate(dot(viewDir, normal));
        fresnel = 1.0f + pow(f, fresnelPower) * fresnelStrength;
    }

    float finalAlpha = baseColor.a * mask * alpha;

    return float4(baseColor.rgb * fresnel, finalAlpha);
}
