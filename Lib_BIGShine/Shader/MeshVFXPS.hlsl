#include "MeshVFX.hlsli"

Texture2D BaseTexture : register(t0);
Texture2D MaskTexture : register(t1);
Texture2D NoiseTexture : register(t2);
Texture2D NormalTexture : register(t3);
Texture2D DistortionNoiseTexture : register(t4);
Texture2D EdgePatternTexture : register(t5);
Texture2D EmissiveTexture : register(t6);
Texture2D SceneColorTexture : register(t7);
Texture2D<float> SceneDepthTexture : register(t8);
Texture2D GradientTexture : register(t9);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearBorderSampler : register(s1);
SamplerState LinearClampSampler : register(s2);

float Hash12(float2 value_)
{
    return frac(sin(dot(value_, float2(127.1f, 311.7f))) * 43758.5453f);
}

float2 ClampUVWithEdgeJitter(Texture2D texture_, float2 uv_, float effectTime_)
{
    uint width_;
    uint height_;
    texture_.GetDimensions(width_, height_);
    float2 textureSize_ = max(float2(width_, height_), float2(1.0f, 1.0f));
    float2 texel_ = 1.0f / textureSize_;
    float2 clampedUV_ = clamp(uv_, texel_ * 0.5f, 1.0f - texel_ * 0.5f);

    float outsideAmount_ = max(
        max(-uv_.x, uv_.x - 1.0f),
        max(-uv_.y, uv_.y - 1.0f));
    float edgeDistance_ = min(
        min(uv_.x, 1.0f - uv_.x),
        min(uv_.y, 1.0f - uv_.y));
    float nearEdgeAmount_ = 1.0f - saturate(edgeDistance_ / max(max(texel_.x, texel_.y) * 2.0f, 0.00001f));
    float jitterAmount_ = saturate(outsideAmount_ * 32.0f + nearEdgeAmount_ * 0.35f);
    if (jitterAmount_ <= 0.0f)
    {
        return clampedUV_;
    }

    float xEdgeAmount_ = max(0.0f, abs(clampedUV_.x - 0.5f) - abs(clampedUV_.y - 0.5f));
    float2 tangent_ = xEdgeAmount_ > 0.0f ? float2(0.0f, 1.0f) : float2(1.0f, 0.0f);
    float stableNoise_ = Hash12(floor(clampedUV_ * textureSize_ * 0.5f));
    float animatedNoise_ = sin(stableNoise_ * 6.2831853f + effectTime_ * 2.7f);
    float2 jitterUV_ = tangent_ * animatedNoise_ * texel_ * 2.0f * jitterAmount_;
    return clamp(clampedUV_ + jitterUV_, texel_ * 0.5f, 1.0f - texel_ * 0.5f);
}

float4 SampleVFXTexture(Texture2D texture_, float2 uv_, uint samplerMode_, float effectTime_)
{
    if (samplerMode_ == 1)
    {
        return texture_.Sample(LinearClampSampler, ClampUVWithEdgeJitter(texture_, uv_, effectTime_));
    }
    if (samplerMode_ == 2)
    {
        return texture_.Sample(LinearBorderSampler, uv_);
    }
    return texture_.Sample(LinearWrapSampler, uv_);
}

static const uint MESH_VFX_GRADIENT_SAMPLE_MODE_HORIZONTAL = 0;
static const uint MESH_VFX_GRADIENT_DIRECTION_FORWARD = 0;

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

float2 ApplyTextureScale(float2 uv, float2 scale)
{
    return uv * max(scale, float2(0.001f, 0.001f));
}

float2 ScaleUVFromCenter(float2 uv, float scale)
{
    // UV中心から拡大縮小し、中心から広がる/縮むようなTexture演出を作る。
    return (uv - uvRotationCenter) / max(scale, 0.001f) + uvRotationCenter;
}

float2 GetGradientUV(float2 sourceUV)
{
    float gradientT = gradientSampleMode == MESH_VFX_GRADIENT_SAMPLE_MODE_HORIZONTAL
        ? sourceUV.x
        : sourceUV.y;
    gradientT = saturate(gradientT);
    if (gradientDirection != MESH_VFX_GRADIENT_DIRECTION_FORWARD)
    {
        gradientT = 1.0f - gradientT;
    }

    gradientT = lerp(gradientStartUv, gradientEndUv, gradientT);
    float fixedUv = saturate(gradientFixedUv);
    return gradientSampleMode == MESH_VFX_GRADIENT_SAMPLE_MODE_HORIZONTAL
        ? float2(gradientT, fixedUv)
        : float2(fixedUv, gradientT);
}

float4 main(VS_OUT input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // Component時間とAfterimageなどのインスタンス時間を足して、このPixelの演出時間を作る。
    float effectTime = time + input.instanceTime;
    float radialScale = max(uvRadialScale + uvRadialScaleSpeed * effectTime, 0.001f);
    float2 baseUV = ScaleUVFromCenter(RotateUV(ApplyTextureScale(input.texcoord, baseUVScale), baseUVRotation), radialScale) + baseUVSpeed * effectTime;
    float2 maskUV = ScaleUVFromCenter(RotateUV(ApplyTextureScale(input.texcoord, maskUVScale), maskUVRotation), radialScale) + maskUVSpeed * effectTime;
    float2 noiseUV = RotateUV(ApplyTextureScale(input.texcoord, noiseUVScale), noiseUVRotation) + noiseUVSpeed * effectTime;
    float2 normalUV = ScaleUVFromCenter(RotateUV(ApplyTextureScale(input.texcoord, normalUVScale), baseUVRotation), radialScale) + baseUVSpeed * effectTime;
    float2 distortionUV = RotateUV(ApplyTextureScale(input.texcoord, distortionNoiseUVScale), noiseUVRotation) + noiseUVSpeed * effectTime;
    float2 edgePatternUV = RotateUV(ApplyTextureScale(input.texcoord, edgePatternUVScale), baseUVRotation) + baseUVSpeed * effectTime;
    float2 emissiveUV = RotateUV(ApplyTextureScale(input.texcoord, emissiveUVScale), baseUVRotation) + baseUVSpeed * effectTime;
    float2 sampledNoise = float2(0.0f, 0.0f);
    float2 distortionNoise = float2(0.0f, 0.0f);

    if (useNoise != 0)
    {
        // Noise TextureのRGを-1～1のUVオフセットとして使い、Base/Maskを同じ方向へ歪ませる。
        sampledNoise = SampleVFXTexture(NoiseTexture, noiseUV, noiseSamplerMode, effectTime).rg * 2.0f - 1.0f;

        if (useNoise != 0)
        {
            baseUV += sampledNoise * noiseStrength;
            maskUV += sampledNoise * noiseStrength;
        }
    }

    // BaseTextureは色として使うだけでなく、Edge Patternの模様元にも使う。
    float4 baseSample = SampleVFXTexture(BaseTexture, baseUV, baseSamplerMode, effectTime);
    float4 baseColor = baseSample * vfxColor;
    if (useGradientTexture != 0 && hasGradientTexture != 0)
    {
        float4 gradientSample = SampleVFXTexture(
            GradientTexture,
            GetGradientUV(input.texcoord),
            gradientSamplerMode,
            effectTime);
        baseColor.rgb = gradientSample.rgb * vfxColor.rgb;
        baseColor.a = baseSample.a * gradientSample.a * vfxColor.a;
    }
    if (useTrailColorRamp != 0)
    {
        float recencyFactor = pow(saturate(1.0f - input.texcoord.y), max(trailRecencyPower, 0.001f));
        float tipFactor = pow(saturate(input.texcoord.x), max(trailTipPower, 0.001f));
        recencyFactor *= saturate(trailRecencyIntensity);
        tipFactor *= saturate(trailTipIntensity);

        float4 oldColor = lerp(trailOldRootColor, trailOldTipColor, tipFactor);
        float4 newColor = lerp(trailNewRootColor, trailNewTipColor, tipFactor);
        float4 trailColor = lerp(oldColor, newColor, recencyFactor);
        trailColor.a *= lerp(saturate(trailOldAlpha), saturate(trailNewAlpha), recencyFactor);
        baseColor *= trailColor;
    }

    float mask = 1.0f;

    if (useMask != 0)
    {
        // Maskは表示/非表示、Dissolve、Edge Patternの参照元として使う。
        mask = SampleVFXTexture(MaskTexture, maskUV, maskSamplerMode, effectTime).r;
    }

    bool distortionEnabled = useMaskDistortion != 0 && hasDistortionNoiseTexture != 0;
    bool edgeEnabled = useEdgeHighlight != 0;
    float hiddenMask = saturate(max(
        max(1.0f - mask, 1.0f - baseColor.a),
        step(mask, dissolveThreshold)));
    if (!distortionEnabled && !edgeEnabled && (mask <= dissolveThreshold || baseColor.a <= 0.0001f))
    {
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
            float3 sampledNormal_ = SampleVFXTexture(
                NormalTexture,
                normalUV,
                normalSamplerMode,
                effectTime).xyz * 2.0f - 1.0f;
            sampledNormal_.xy *= normalStrength;
            normal = normalize(mul(sampledNormal_, tangentBasis_));
        }

        // 視線と法線が直交するほど強くなるため、Meshの外周に光が乗る。
        float f = 1.0f - saturate(abs(dot(viewDir, normal)));
        fresnelFactor = pow(f, fresnelPower) * fresnelStrength;
        if (useFresnelNoise != 0)
        {
            float fresnelNoiseSample_ = SampleVFXTexture(
                NoiseTexture,
                noiseUV,
                noiseSamplerMode,
                effectTime).r;
            fresnelFactor *= lerp(1.0f, saturate(fresnelNoiseSample_ * 2.0f), saturate(fresnelNoiseStrength));
        }
    }

    float3 finalColor = baseColor.rgb + vfxColor.rgb * fresnelFactor;
    float finalAlpha = distortionEnabled
        ? baseColor.a * mask * alpha * input.instanceAlpha * (1.0f - hiddenMask)
        : baseColor.a * mask * alpha * input.instanceAlpha;
    if (useFresnel != 0)
    {
        finalAlpha = saturate(finalAlpha + fresnelFactor * mask * alpha * input.instanceAlpha);
    }

    if (useDissolveEdge != 0 && dissolveEdgeStrength > 0.0f && dissolveEdgeWidth > 0.0f)
    {
        float dissolveSource_ = min(mask, baseColor.a);
        float dissolveDistance_ = dissolveSource_ - dissolveThreshold;
        float dissolveEdgeAmount_ =
            (1.0f - smoothstep(0.0f, dissolveEdgeWidth, dissolveDistance_)) *
            step(0.0f, dissolveDistance_);
        float dissolveEdgeAlpha_ = dissolveEdgeColor.a * dissolveEdgeAmount_ * alpha * input.instanceAlpha;
        finalColor += dissolveEdgeColor.rgb * dissolveEdgeStrength * dissolveEdgeAmount_;
        finalAlpha = saturate(max(finalAlpha, dissolveEdgeAlpha_));
    }

    if (emissiveStrength > 0.0f)
    {
        float4 emissiveSample = hasEmissiveTexture != 0
            ? SampleVFXTexture(EmissiveTexture, emissiveUV, emissiveSamplerMode, effectTime)
            : baseSample;
        float emissivePulse = 1.0f + sin(effectTime * emissivePulseSpeed) * emissivePulseAmount;
        finalColor += emissiveSample.rgb * vfxColor.rgb * max(emissiveStrength, 0.0f) * max(emissivePulse, 0.0f) * saturate(emissiveSample.a * finalAlpha);
    }

    if (useBackFaceBoost != 0 && !isFrontFace && backFaceEmissiveStrength > 0.0f)
    {
        float backFaceAmount_ = saturate(backFaceColor.a);
        finalColor += backFaceColor.rgb * backFaceEmissiveStrength * backFaceAmount_;
        finalAlpha = saturate(max(finalAlpha, baseColor.a * mask * alpha * input.instanceAlpha * backFaceAmount_));
    }

    if (edgeEnabled)
    {
        // 頂点を押し出さず、Mesh表面の内側だけで外周っぽい部分を強調する。
        float silhouetteFactor_ = 1.0f - saturate(abs(dot(viewDir, normal)));
        float silhouetteCoverage_ = max(fwidth(silhouetteFactor_) * 1.5f, 0.02f);
        float silhouetteEdge =
            pow(saturate(silhouetteFactor_ + silhouetteCoverage_), silhouetteEdgePower) *
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
            float4 edgeSample = hasEdgePatternTexture != 0
                ? SampleVFXTexture(EdgePatternTexture, edgePatternUV, edgePatternSamplerMode, effectTime)
                : baseSample;
            edgePattern = saturate(dot(edgeSample.rgb, float3(0.299f, 0.587f, 0.114f)) * edgeSample.a);
        }
        else if (edgePatternMode == 2)
        {
            edgePattern = hasEdgePatternTexture != 0
                ? SampleVFXTexture(EdgePatternTexture, edgePatternUV, edgePatternSamplerMode, effectTime).r
                : (useMask != 0 ? mask : 1.0f);
        }

        edgePattern = lerp(1.0f, edgePattern, saturate(edgePatternStrength));

        float edgeAmount = saturate(silhouetteEdge + intersectionEdge) * edgePattern;
        float edgeAlpha = edgeColor.a * edgeAmount * alpha * input.instanceAlpha;
        finalColor = lerp(finalColor, edgeColor.rgb, saturate(edgeAmount * edgeColor.a));
        finalAlpha = saturate(max(finalAlpha, edgeAlpha));
    }

    if (layerMaskPass != 0)
    {
        clip(finalAlpha - 0.01f);
    }
    return float4(finalColor, finalAlpha);
}
