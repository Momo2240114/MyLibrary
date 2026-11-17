#include "ModelCommon.hlsli"
#include "gbuffer_functions.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

static const int SHADING_MODEL_UNLIT = 0;
static const int SHADING_MODEL_PBR = 1;
static const int SHADING_MODEL_TOON = 2;
static const int SHADING_MODEL_TOON_PBR = 3;

Texture2D GBufferBaseColor : register(t0);
Texture2D GBufferNormal : register(t1);
Texture2D GBufferMaterial : register(t2);
Texture2D GBufferEmissive : register(t3);
Texture2D SceneDepth : register(t4);
Texture2D GBufferStyle : register(t7);
TextureCube DiffuseIem : register(t10);
TextureCube SpecularPmrem : register(t11);
Texture2D LutGgx : register(t12);
SamplerState LinearSampler : register(s0);

cbuffer CbDeferredSettings : register(b9)
{
    float4 toonShadeSteps;
    float4 toonShadeFeathers;
    float4 toonPbrShadeSteps;
    float4 toonPbrShadeFeathers;
};

struct VS_OUT_FULLSCREEN
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float3 ACESFilm(float3 color_)
{
    return saturate((color_ * (2.51f * color_ + 0.03f)) / (color_ * (2.43f * color_ + 0.59f) + 0.14f));
}

float ToonRamp(float value_, int shadingModel_)
{
    float4 shadeSteps_ = shadingModel_ == SHADING_MODEL_TOON ? toonShadeSteps : toonPbrShadeSteps;
    float4 shadeFeathers_ = shadingModel_ == SHADING_MODEL_TOON ? toonShadeFeathers : toonPbrShadeFeathers;
    float amount_ = 0.0f;
    amount_ += smoothstep(shadeSteps_.x - shadeFeathers_.x, shadeSteps_.x + shadeFeathers_.x, value_);
    amount_ += smoothstep(shadeSteps_.y - shadeFeathers_.y, shadeSteps_.y + shadeFeathers_.y, value_);
    amount_ += smoothstep(shadeSteps_.z - shadeFeathers_.z, shadeSteps_.z + shadeFeathers_.z, value_);
    amount_ += smoothstep(shadeSteps_.w - shadeFeathers_.w, shadeSteps_.w + shadeFeathers_.w, value_);
    return amount_ * 0.25f;
}

float3 CalculateDirectLight(float3 baseColor_, float3 normal_, float3 view_, float3 light_, float3 lightColor_,
    float metalness_, float roughness_, int shadingModel_, float toonPbrBlend_,
    float3 shadowTint_, float specularToonBlend_, float specularStrength_)
{
    float3 halfVector_ = normalize(light_ + view_);
    float nDotL_ = saturate(dot(normal_, light_));
    float nDotV_ = abs(dot(normal_, view_)) + 1e-5f;
    float nDotH_ = saturate(dot(normal_, halfVector_));
    float vDotH_ = saturate(dot(view_, halfVector_));
    float3 f0_ = lerp(float3(0.04f, 0.04f, 0.04f), baseColor_, metalness_);
    float3 f90_ = 1.0f;
    float3 diffuse_ = brdf_lambertian(f0_, f90_, baseColor_, vDotH_) * (1.0f - metalness_);
    float3 specular_ = brdf_specular_ggx(f0_, f90_, max(roughness_ * roughness_, 0.001f), vDotH_, nDotL_, nDotV_, nDotH_);
    float3 pbr_ = (diffuse_ + specular_) * lightColor_ * nDotL_;

    float toonAmount_ = ToonRamp(nDotL_, shadingModel_);
    float3 toonShadow_ = baseColor_ * shadowTint_;
    float3 toonDiffuse_ = lerp(toonShadow_, baseColor_, toonAmount_) * (1.0f - metalness_);
    float3 toonSpecular_ = specular_ * step(0.4f, max(specular_.r, max(specular_.g, specular_.b)));
    float3 toon_ = (toonDiffuse_ + toonSpecular_) * lightColor_ * toonAmount_;

    if (shadingModel_ == SHADING_MODEL_TOON)
    {
        return toon_;
    }
    if (shadingModel_ == SHADING_MODEL_TOON_PBR)
    {
        // 拡散光はセル調、反射はPBR寄りに個別調整する。
        float3 diffuseResult_ = lerp(diffuse_ * nDotL_, toonDiffuse_ * toonAmount_, toonPbrBlend_);
        float specularMask_ = smoothstep(0.25f, 0.75f,
            saturate(max(specular_.r, max(specular_.g, specular_.b)) * (1.0f - roughness_)));
        float3 softToonSpecular_ = specular_ * lerp(0.35f, 1.0f, specularMask_);
        float3 specularResult_ = lerp(specular_, softToonSpecular_, saturate(specularToonBlend_));
        return (diffuseResult_ + specularResult_ * nDotL_ * specularStrength_) * lightColor_;
    }
    return pbr_;
}

float4 main(VS_OUT_FULLSCREEN pin_) : SV_TARGET
{
    float depth_ = SceneDepth.Sample(LinearSampler, pin_.uv).r;
    if (depth_ >= 0.999999f)
    {
        return 0.0f;
    }

    float4 baseAndModel_ = GBufferBaseColor.Sample(LinearSampler, pin_.uv);
    float3 baseColor_ = baseAndModel_.rgb;
    int shadingModel_ = (int)round(baseAndModel_.a);
    float3 emissive_ = GBufferEmissive.Sample(LinearSampler, pin_.uv).rgb;
    if (shadingModel_ == SHADING_MODEL_UNLIT)
    {
        return float4(baseColor_ + emissive_, 1.0f);
    }

    float4 normalAndSpecular_ = GBufferNormal.Sample(LinearSampler, pin_.uv);
    float3 normal_ = normalize(normalAndSpecular_.xyz * 2.0f - 1.0f);
    float4 style_ = GBufferStyle.Sample(LinearSampler, pin_.uv);
    float specularStrength_ = max(normalAndSpecular_.a, 0.0f);
    float4 material_ = GBufferMaterial.Sample(LinearSampler, pin_.uv);
    float ao_ = material_.r;
    float roughness_ = material_.g;
    float metalness_ = material_.b;
    float toonPbrBlend_ = material_.a;
    float3 worldPosition_ = ReconstructWorldPos(pin_.uv, depth_, invViewProjection);
    float3 view_ = normalize(cameraPosition.xyz - worldPosition_);
    float shadowVisibility_ = CalcCascadeShadowVisibility(worldPosition_);
    float indirectShadow_ = lerp(cascadeIndirectShadowAttenuation, 1.0f, shadowVisibility_);

    float3 directLighting_ = CalculateDirectLight(baseColor_, normal_, view_, normalize(-lightDirection.xyz),
        lightColor.rgb * lightDirection.w * lerp(cascadeShadowAttenuation, 1.0f, shadowVisibility_),
        metalness_, roughness_, shadingModel_, toonPbrBlend_, style_.rgb, style_.a, specularStrength_);

    [loop]
    for (int pointIndex_ = 0; pointIndex_ < numPointLights; ++pointIndex_)
    {
        PointLightData pointLight_ = pointLights[pointIndex_];
        float3 lightVector_ = pointLight_.position - worldPosition_;
        float distance_ = length(lightVector_);
        if (distance_ > pointLight_.range)
        {
            continue;
        }
        float attenuation_ = saturate(1.0f - distance_ / pointLight_.range);
        attenuation_ *= attenuation_;
        directLighting_ += CalculateDirectLight(baseColor_, normal_, view_, normalize(lightVector_),
            pointLight_.color * pointLight_.intensity * attenuation_,
            metalness_, roughness_, shadingModel_, toonPbrBlend_, style_.rgb, style_.a, specularStrength_);
    }

    [loop]
    for (int spotIndex_ = 0; spotIndex_ < numSpotLights; ++spotIndex_)
    {
        SpotLightData spotLight_ = spotLights[spotIndex_];
        float3 lightVector_ = spotLight_.position - worldPosition_;
        float distance_ = length(lightVector_);
        if (distance_ > spotLight_.range)
        {
            continue;
        }
        float3 light_ = normalize(lightVector_);
        float cosAngle_ = dot(normalize(spotLight_.direction), -light_);
        float cosInner_ = cos(radians(spotLight_.innerAngle));
        float cosOuter_ = cos(radians(spotLight_.outerAngle));
        float spotAttenuation_ = saturate((cosAngle_ - cosOuter_) / max(cosInner_ - cosOuter_, 1e-4f));
        float attenuation_ = saturate(1.0f - distance_ / spotLight_.range);
        attenuation_ = attenuation_ * attenuation_ * spotAttenuation_ * spotAttenuation_;
        directLighting_ += CalculateDirectLight(baseColor_, normal_, view_, light_,
            spotLight_.color * spotLight_.intensity * attenuation_,
            metalness_, roughness_, shadingModel_, toonPbrBlend_, style_.rgb, style_.a, specularStrength_);
    }

    float3 f0_ = lerp(float3(0.04f, 0.04f, 0.04f), baseColor_, metalness_);
    float nDotV_ = max(0.0001f, dot(normal_, view_));
    float3 fresnel_ = f0_ + (max((float3)(1.0f - roughness_), f0_) - f0_) * pow(1.0f - nDotV_, 5.0f);
    float3 diffuseIbl_ = baseColor_ * (1.0f - metalness_) * DiffuseIem.Sample(LinearSampler, normal_).rgb * (1.0f - fresnel_);
    float3 reflection_ = normalize(reflect(-view_, normal_));
    uint width_;
    uint height_;
    uint mipCount_;
    SpecularPmrem.GetDimensions(0, width_, height_, mipCount_);
    float3 specularLight_ = SpecularPmrem.SampleLevel(LinearSampler, reflection_, roughness_ * (mipCount_ - 1)).rgb;
    float2 environmentBrdf_ = LutGgx.Sample(LinearSampler, float2(nDotV_, roughness_)).rg;
    float3 specularIbl_ = specularLight_ * (f0_ * environmentBrdf_.x + environmentBrdf_.y);

    float toonBlend_ = shadingModel_ == SHADING_MODEL_TOON ? 1.0f : (shadingModel_ == SHADING_MODEL_TOON_PBR ? toonPbrBlend_ : 0.0f);
    float3 finalColor_ = directLighting_ + diffuseIbl_ * ao_ * lerp(1.0f, 0.4f, toonBlend_) * indirectShadow_
        + specularIbl_ * (shadingModel_ == SHADING_MODEL_TOON_PBR
            ? lerp(1.0f, 0.35f, style_.a) * specularStrength_
            : lerp(1.0f, 0.03f, toonBlend_)) + emissive_;
    // 発光と反射を線形HDRのまま後段へ渡し、ToneMappingの重複を避ける。
    if (shadingModel_ == SHADING_MODEL_TOON_PBR)
    {
        float rimStrength_ = GBufferEmissive.Sample(LinearSampler, pin_.uv).a;
        float rim_ = pow(1.0f - saturate(abs(dot(normal_, view_))), 3.0f) * max(rimStrength_, 0.0f);
        finalColor_ += rim_ * lerp(baseColor_, float3(1.0f, 1.0f, 1.0f), 0.5f);
    }
    finalColor_ = max(finalColor_, 0.0f);
    return float4(finalColor_, 1.0f);
}
