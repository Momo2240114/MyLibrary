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
    // 指定中心を基準にUVを回転する。斬撃模様や流れるMaskの向きを変えるために使う。
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
    // UV中心から拡大縮小し、中心から広がる/縮むようなTexture演出を作る。
    return (uv - uvRotationCenter) / max(scale, 0.001f) + uvRotationCenter;
}

float4 main(VS_OUT input) : SV_TARGET
{
    // Component時間とAfterimageなどのインスタンス時間を足して、このPixelの演出時間を作る。
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
        // Noise TextureのRGを-1～1のUVオフセットとして使い、Base/Maskを同じ方向へ歪ませる。
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
        // 背景歪み専用Noise。Maskの透明側でScene Colorをずらすために使う。
        distortionNoise = (abs(noiseUVRotation) > 0.0001f
            ? DistortionNoiseTexture.Sample(LinearBorderSampler, noiseUV)
            : DistortionNoiseTexture.Sample(LinearWrapSampler, noiseUV)).rg * 2.0f - 1.0f;
    }

    // BaseTextureは色として使うだけでなく、Edge Patternの模様元にも使う。
    float4 baseSample = abs(baseUVRotation) > 0.0001f
        ? BaseTexture.Sample(LinearBorderSampler, baseUV)
        : BaseTexture.Sample(LinearWrapSampler, baseUV);
    float4 baseColor = baseSample * vfxColor;

    float mask = 1.0f;

    if (useMask != 0)
    {
        // Maskは表示/非表示、Dissolve、Edge Patternの参照元として使う。
        mask = abs(maskUVRotation) > 0.0001f
            ? MaskTexture.Sample(LinearBorderSampler, maskUV).r
            : MaskTexture.Sample(LinearWrapSampler, maskUV).r;
    }

    if (!distortionEnabled && (mask <= dissolveThreshold || baseColor.a <= 0.0001f))
    {
        // 歪み表現を使わない場合は、見えないPixelを早めに破棄して描画負荷と不要なAlphaを減らす。
        discard;
    }

    float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
    float3 normal = normalize(input.normal);
    float fresnelFactor = 0.0f;

    if (useFresnel != 0)
    {
        // Normal Textureがある場合はTBN行列で法線を補正し、Fresnelの形に反映する。
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

        // 視線と法線が直交するほど強くなるため、Meshの外周に光が乗る。
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
        // Maskで隠れる側だけScene ColorをNoise方向へずらし、熱気や空間の揺らぎを表現する。
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
        // 頂点を押し出さず、Mesh表面の内側だけで外周っぽい部分を強調する。
        float silhouetteEdge =
            pow(1.0f - saturate(abs(dot(viewDir, normal))), silhouetteEdgePower) *
            silhouetteEdgeStrength;

        float intersectionEdge = 0.0f;
        if (intersectionEdgeStrength > 0.0f)
        {
            // Scene Depthとの差が小さい場所を交差Edgeとして扱う。地面や敵に重なる境界を拾う。
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
        float edgePattern = 1.0f;
        if (edgePatternMode == 1)
        {
            // BaseTextureの明度とAlphaでEdgeの中に模様を作る。
            edgePattern = saturate(
                dot(baseSample.rgb, float3(0.299f, 0.587f, 0.114f)) *
                baseSample.a);
        }
        else if (edgePatternMode == 2 && useMask != 0)
        {
            // Mask値をそのままEdge模様として使う。
            edgePattern = mask;
        }

        edgePattern = lerp(1.0f, edgePattern, saturate(edgePatternStrength));

        float edgeAmount = saturate(silhouetteEdge + intersectionEdge) * edgePattern;
        finalColor += edgeColor.rgb * edgeAmount;
        finalAlpha = saturate(
            finalAlpha + edgeColor.a * edgeAmount * input.instanceAlpha);
    }

    return float4(finalColor, finalAlpha);
}
