#include "MeshVFX.hlsli"

Texture2D BaseTexture : register(t0);
Texture2D MaskTexture : register(t1);
Texture2D NoiseTexture : register(t2);
Texture2D NormalTexture : register(t3);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearBorderSampler : register(s1);

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
        float2 noise = (abs(noiseUVRotation) > 0.0001f
            ? NoiseTexture.Sample(LinearBorderSampler, noiseUV)
            : NoiseTexture.Sample(LinearWrapSampler, noiseUV)).rg * 2.0f - 1.0f;

        baseUV += noise * noiseStrength;
        maskUV += noise * noiseStrength;
    }

    float4 baseColor = abs(baseUVRotation) > 0.0001f
        ? BaseTexture.Sample(LinearBorderSampler, baseUV)
        : BaseTexture.Sample(LinearWrapSampler, baseUV);
    baseColor *= vfxColor;

    float mask = 1.0f;

    if (useMask != 0)
    {
        mask = abs(maskUVRotation) > 0.0001f
            ? MaskTexture.Sample(LinearBorderSampler, maskUV).r
            : MaskTexture.Sample(LinearWrapSampler, maskUV).r;
    }

    if (mask <= dissolveThreshold || baseColor.a <= 0.0001f)
    {
        discard;
    }

    float fresnelFactor = 0.0f;

    if (useFresnel != 0)
    {
        float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
        float3 normal = normalize(input.normal);

        if (useNormal != 0)
        {
            float3 tangent_ = normalize(input.tangent.xyz);
            float3 bitangent_ = normalize(cross(normal, tangent_)) * input.tangent.w;
            float3x3 tangentBasis_ = float3x3(tangent_, bitangent_, normal);
            float3 sampledNormal_ = (abs(baseUVRotation) > 0.0001f
                ? NormalTexture.Sample(LinearBorderSampler, baseUV)
                : NormalTexture.Sample(LinearWrapSampler, baseUV)).xyz * 2.0f - 1.0f;
            sampledNormal_.xy *= normalStrength;
            normal = normalize(mul(sampledNormal_, tangentBasis_));
        }

        float f = 1.0f - saturate(abs(dot(viewDir, normal)));
        fresnelFactor = pow(f, fresnelPower) * fresnelStrength;
    }

    float3 finalColor = baseColor.rgb + vfxColor.rgb * fresnelFactor;
    float finalAlpha = baseColor.a * mask * alpha;
    if (useFresnel != 0)
    {
        finalAlpha = saturate(finalAlpha + fresnelFactor * mask * alpha);
    }

    return float4(finalColor, finalAlpha);
}
