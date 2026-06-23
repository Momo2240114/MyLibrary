#include "ModelCommon.hlsli"

cbuffer CbGBufferMaterial : register(b0)
{
    float4 baseColor;
    float3 emissiveColor;
    float metalnessParam;
    float roughnessParam;
    float occlusionStrength;
    float toonPbrBlend;
    int shadingModel;
};

cbuffer CbMaterialOverlay : register(b1)
{
    float4 overlayColor;
    float2 overlayMaskUVSpeed;
    float2 overlayNoiseUVSpeed;
    float overlayTime;
    float overlayAlpha;
    float overlayNoiseStrength;
    float overlayDissolveThreshold;
    uint overlayEnabled;
    uint overlayUseMask;
    uint overlayUseNoise;
    float overlayEmissiveStrength;
    float overlayEmissivePulseSpeed;
    float overlayEmissivePulseAmount;
    float2 overlayPadding;
};

Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);
Texture2D OverlayMaskMap : register(t9);
Texture2D OverlayNoiseMap : register(t10);
SamplerState LinearSampler : register(s0);

struct GBUFFER_OUT
{
    float4 baseColorAndModel : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
    float4 emissive : SV_TARGET3;
};

GBUFFER_OUT main(VS_OUT pin)
{
    float2 materialUV_ = pin.texcoord;
    float overlayAmount_ = 0.0f;
    if (overlayEnabled != 0)
    {
        float2 noiseOffset_ = 0.0f;
        if (overlayUseNoise != 0)
        {
            noiseOffset_ = OverlayNoiseMap.Sample(
                LinearSampler,
                pin.texcoord + overlayNoiseUVSpeed * overlayTime).rg * 2.0f - 1.0f;
        }
        const float2 distortedUV_ = pin.texcoord + noiseOffset_ * overlayNoiseStrength;
        const float mask_ = overlayUseMask != 0
            ? OverlayMaskMap.Sample(
                LinearSampler,
                distortedUV_ + overlayMaskUVSpeed * overlayTime).r
            : 1.0f;
        const float visibleMask_ = saturate(
            (mask_ - overlayDissolveThreshold) /
            max(1.0f - overlayDissolveThreshold, 0.001f));
        overlayAmount_ = saturate(visibleMask_ * overlayAlpha);
        materialUV_ = lerp(pin.texcoord, distortedUV_, overlayAmount_);
    }

    float4 sampledBase_ = BaseMap.Sample(LinearSampler, materialUV_) * baseColor;
    sampledBase_.rgb = lerp(
        sampledBase_.rgb,
        sampledBase_.rgb * overlayColor.rgb,
        overlayAmount_ * overlayColor.a);
    if (sampledBase_.a < 0.1f)
    {
        discard;
    }

    float3 normal_ = normalize(pin.normal);
    float3 tangent_ = normalize(pin.tangent);
    float3 bitangent_ = normalize(cross(normal_, tangent_));
    float3x3 tangentBasis_ = float3x3(tangent_, bitangent_, normal_);
    float3 sampledNormal_ = NormalMap.Sample(LinearSampler, materialUV_).xyz * 2.0f - 1.0f;
    normal_ = normalize(mul(sampledNormal_, tangentBasis_));

    float2 metalRough_ = MetalRoughMap.Sample(LinearSampler, materialUV_).gr;
    float metalness_ = saturate(metalRough_.r * metalnessParam);
    float roughness_ = max(saturate(metalRough_.g * roughnessParam), 0.04f);
    float ao_ = lerp(1.0f, AOMap.Sample(LinearSampler, materialUV_).r, occlusionStrength);
    float3 emissive_ = EmissiveMap.Sample(LinearSampler, materialUV_).rgb * emissiveColor;
    const float pulsePhase_ = overlayTime * overlayEmissivePulseSpeed * 6.28318530718f;
    const float pulse_ = 1.0f +
        (sin(pulsePhase_) * 0.5f + 0.5f) * overlayEmissivePulseAmount;
    emissive_ += overlayColor.rgb * overlayAmount_ * overlayEmissiveStrength * pulse_;

    GBUFFER_OUT output_;
    output_.baseColorAndModel = float4(sampledBase_.rgb, shadingModel);
    output_.normal = float4(normal_ * 0.5f + 0.5f, 1.0f);
    output_.material = float4(ao_, roughness_, metalness_, saturate(toonPbrBlend));
    output_.emissive = float4(emissive_, 1.0f);
    return output_;
}
