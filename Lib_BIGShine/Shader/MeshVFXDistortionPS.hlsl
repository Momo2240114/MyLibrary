#include "MeshVFX.hlsli"

Texture2D BaseTexture : register(t0);
Texture2D MaskTexture : register(t1);
Texture2D NoiseTexture : register(t2);
Texture2D DistortionNoiseTexture : register(t4);
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
    if (samplerMode_ == 1) return texture_.Sample(LinearClampSampler, ClampUVWithEdgeJitter(texture_, uv_, effectTime_));
    if (samplerMode_ == 2) return texture_.Sample(LinearBorderSampler, uv_);
    return texture_.Sample(LinearWrapSampler, uv_);
}

static const uint MESH_VFX_GRADIENT_SAMPLE_MODE_HORIZONTAL = 0;
static const uint MESH_VFX_GRADIENT_DIRECTION_FORWARD = 0;

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
        float2 sampledNoise = SampleVFXTexture(
            NoiseTexture,
            noiseUV,
            noiseSamplerMode,
            effectTime).rg * 2.0f - 1.0f;
        baseUV += sampledNoise * noiseStrength;
        maskUV += sampledNoise * noiseStrength;
    }

    float4 baseSample = SampleVFXTexture(BaseTexture, baseUV, baseSamplerMode, effectTime);
    if (useGradientTexture != 0 && hasGradientTexture != 0)
    {
        baseSample.a *= SampleVFXTexture(
            GradientTexture,
            GetGradientUV(input.texcoord),
            gradientSamplerMode,
            effectTime).a;
    }

    float mask = 1.0f;
    if (useMask != 0)
    {
        mask = SampleVFXTexture(MaskTexture, maskUV, maskSamplerMode, effectTime).r;
    }

    float hiddenMask = saturate(max(
        max(1.0f - mask, 1.0f - baseSample.a),
        step(mask, dissolveThreshold)));

    if (hiddenMask <= 0.0001f)
    {
        discard;
    }

    float2 distortionNoise = SampleVFXTexture(
        DistortionNoiseTexture,
        distortionUV,
        distortionSamplerMode,
        effectTime).rg * 2.0f - 1.0f;

    float opacity = saturate(maskDistortionOpacity * input.instanceAlpha * hiddenMask);
    return float4(distortionNoise * maskDistortionStrength, 0.0f, opacity);
}
