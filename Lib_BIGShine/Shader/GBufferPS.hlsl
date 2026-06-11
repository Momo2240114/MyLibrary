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

Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);
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
    float4 sampledBase_ = BaseMap.Sample(LinearSampler, pin.texcoord) * baseColor;
    if (sampledBase_.a < 0.1f)
    {
        discard;
    }

    float3 normal_ = normalize(pin.normal);
    float3 tangent_ = normalize(pin.tangent);
    float3 bitangent_ = normalize(cross(normal_, tangent_));
    float3x3 tangentBasis_ = float3x3(tangent_, bitangent_, normal_);
    float3 sampledNormal_ = NormalMap.Sample(LinearSampler, pin.texcoord).xyz * 2.0f - 1.0f;
    normal_ = normalize(mul(sampledNormal_, tangentBasis_));

    float2 metalRough_ = MetalRoughMap.Sample(LinearSampler, pin.texcoord).gr;
    float metalness_ = saturate(metalRough_.r * metalnessParam);
    float roughness_ = max(saturate(metalRough_.g * roughnessParam), 0.04f);
    float ao_ = lerp(1.0f, AOMap.Sample(LinearSampler, pin.texcoord).r, occlusionStrength);
    float3 emissive_ = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveColor;

    GBUFFER_OUT output_;
    output_.baseColorAndModel = float4(sampledBase_.rgb, shadingModel);
    output_.normal = float4(normal_ * 0.5f + 0.5f, 1.0f);
    output_.material = float4(ao_, roughness_, metalness_, saturate(toonPbrBlend));
    output_.emissive = float4(emissive_, 1.0f);
    return output_;
}
