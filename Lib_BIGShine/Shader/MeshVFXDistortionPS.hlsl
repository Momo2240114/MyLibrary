#include "MeshVFX.hlsli"

Texture2D BaseTexture : register(t0);
Texture2D MaskTexture : register(t1);
Texture2D NoiseTexture : register(t2);
Texture2D DistortionNoiseTexture : register(t4);

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

float2 ScaleUVFromCenter(float2 uv, float scale)
{
    return (uv - uvRotationCenter) / max(scale, 0.001f) + uvRotationCenter;
}

float2 ApplyTextureScale(float2 uv, float2 scale)
{
    return uv * max(scale, float2(0.001f, 0.001f));
}

float4 main(VS_OUT input) : SV_TARGET
{
    if (useMaskDistortion == 0 || hasDistortionNoiseTexture == 0)
    {
        discard;
    }

    float effectTime = time + input.instanceTime;
    float radialScale = max(uvRadialScale + uvRadialScaleSpeed * effectTime, 0.001f);
    float2 baseUV = ScaleUVFromCenter(RotateUV(ApplyTextureScale(input.texcoord, baseUVScale), baseUVRotation), radialScale) + baseUVSpeed * effectTime;
    float2 maskUV = ScaleUVFromCenter(RotateUV(ApplyTextureScale(input.texcoord, maskUVScale), maskUVRotation), radialScale) + maskUVSpeed * effectTime;
    float2 noiseUV = RotateUV(ApplyTextureScale(input.texcoord, noiseUVScale), noiseUVRotation) + noiseUVSpeed * effectTime;
    float2 distortionUV = RotateUV(ApplyTextureScale(input.texcoord, distortionNoiseUVScale), noiseUVRotation) + noiseUVSpeed * effectTime;

    if (useNoise != 0)
    {
        float2 sampledNoise = (abs(noiseUVRotation) > 0.0001f
            ? NoiseTexture.Sample(LinearBorderSampler, noiseUV)
            : NoiseTexture.Sample(LinearWrapSampler, noiseUV)).rg * 2.0f - 1.0f;
        baseUV += sampledNoise * noiseStrength;
        maskUV += sampledNoise * noiseStrength;
    }

    float4 baseSample = abs(baseUVRotation) > 0.0001f
        ? BaseTexture.Sample(LinearBorderSampler, baseUV)
        : BaseTexture.Sample(LinearWrapSampler, baseUV);

    float mask = 1.0f;
    if (useMask != 0)
    {
        mask = abs(maskUVRotation) > 0.0001f
            ? MaskTexture.Sample(LinearBorderSampler, maskUV).r
            : MaskTexture.Sample(LinearWrapSampler, maskUV).r;
    }

    float hiddenMask = saturate(max(
        max(1.0f - mask, 1.0f - baseSample.a),
        step(mask, dissolveThreshold)));

    if (hiddenMask <= 0.0001f)
    {
        discard;
    }

    float2 distortionNoise = (abs(noiseUVRotation) > 0.0001f
        ? DistortionNoiseTexture.Sample(LinearBorderSampler, distortionUV)
        : DistortionNoiseTexture.Sample(LinearWrapSampler, distortionUV)).rg * 2.0f - 1.0f;

    float opacity = saturate(maskDistortionOpacity * input.instanceAlpha * hiddenMask);
    return float4(distortionNoise * maskDistortionStrength, 0.0f, opacity);
}
