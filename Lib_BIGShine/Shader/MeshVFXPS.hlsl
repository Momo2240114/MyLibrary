#include "MeshVFX.hlsli"

Texture2D BaseTexture : register(t0);
Texture2D MaskTexture : register(t1);
Texture2D NoiseTexture : register(t2);
Texture2D NormalTexture : register(t3);
Texture2D DistortionNoiseTexture : register(t4);
Texture2D SceneColorTexture : register(t7);
Texture2D<float> SceneDepthTexture : register(t8);

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

float4 main(VS_OUT input) : SV_TARGET
{
    float effectTime = time + input.instanceTime;
    float radialScale = max(uvRadialScale + uvRadialScaleSpeed * effectTime, 0.001f);
    float2 baseUV = ScaleUVFromCenter(RotateUV(input.texcoord, baseUVRotation), radialScale) + baseUVSpeed * effectTime;
    float2 maskUV = ScaleUVFromCenter(RotateUV(input.texcoord, maskUVRotation), radialScale) + maskUVSpeed * effectTime;
    float2 noiseUV = RotateUV(input.texcoord, noiseUVRotation) + noiseUVSpeed * effectTime;
    float2 sampledNoise = float2(0.0f, 0.0f);
    float2 distortionNoise = float2(0.0f, 0.0f);
    bool distortionEnabled = useMaskDistortion != 0 && hasDistortionNoiseTexture != 0;

    if (useNoise != 0)
    {
        sampledNoise = (abs(noiseUVRotation) > 0.0001f
            ? NoiseTexture.Sample(LinearBorderSampler, noiseUV)
            : NoiseTexture.Sample(LinearWrapSampler, noiseUV)).rg * 2.0f - 1.0f;

        if (useNoise != 0)
        {
            baseUV += sampledNoise * noiseStrength;
            maskUV += sampledNoise * noiseStrength;
        }
    }
    if (distortionEnabled)
    {
        distortionNoise = (abs(noiseUVRotation) > 0.0001f
            ? DistortionNoiseTexture.Sample(LinearBorderSampler, noiseUV)
            : DistortionNoiseTexture.Sample(LinearWrapSampler, noiseUV)).rg * 2.0f - 1.0f;
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

    if (!distortionEnabled && (mask <= dissolveThreshold || baseColor.a <= 0.0001f))
    {
        discard;
    }

    float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
    float3 normal = normalize(input.normal);
    float fresnelFactor = 0.0f;

    if (useFresnel != 0)
    {
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
    float finalAlpha = baseColor.a * mask * alpha * input.instanceAlpha;
    if (useFresnel != 0)
    {
        finalAlpha = saturate(finalAlpha + fresnelFactor * mask * alpha * input.instanceAlpha);
    }

    if (distortionEnabled)
    {
        uint sceneWidth;
        uint sceneHeight;
        SceneColorTexture.GetDimensions(sceneWidth, sceneHeight);
        float2 screenSize = max(float2(sceneWidth, sceneHeight), float2(1.0f, 1.0f));
        float2 screenUV = input.position.xy / screenSize;
        float2 distortedUV = screenUV + distortionNoise * maskDistortionStrength / screenSize;
        float3 distortedScene = SceneColorTexture.Sample(LinearBorderSampler, distortedUV).rgb;
        float hiddenMask = saturate(max(
            max(1.0f - mask, 1.0f - baseColor.a),
            step(mask, dissolveThreshold)));
        finalColor = lerp(finalColor, distortedScene, hiddenMask);
        finalAlpha = lerp(
            finalAlpha,
            maskDistortionOpacity * input.instanceAlpha,
            hiddenMask);
    }

    if (useEdgeHighlight != 0)
    {
        float silhouetteEdge =
            pow(1.0f - saturate(abs(dot(viewDir, normal))), silhouetteEdgePower) *
            silhouetteEdgeStrength;

        float intersectionEdge = 0.0f;
        if (intersectionEdgeStrength > 0.0f)
        {
            uint depthWidth;
            uint depthHeight;
            SceneDepthTexture.GetDimensions(depthWidth, depthHeight);
            float2 depthSize = max(float2(depthWidth, depthHeight), float2(1.0f, 1.0f));
            float2 screenUV = input.position.xy / depthSize;
            float sceneDepth = SceneDepthTexture.Sample(LinearBorderSampler, screenUV);
            float depthDifference = abs(sceneDepth - input.position.z);
            intersectionEdge =
                (1.0f - smoothstep(0.0f, intersectionEdgeWidth, depthDifference)) *
                intersectionEdgeStrength;
        }
        float edgeAmount = saturate(silhouetteEdge + intersectionEdge);
        finalColor += edgeColor.rgb * edgeAmount;
        finalAlpha = saturate(
            finalAlpha + edgeColor.a * edgeAmount * input.instanceAlpha);
    }

    return float4(finalColor, finalAlpha);
}
