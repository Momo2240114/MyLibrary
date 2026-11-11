// ToonPBRPS.hlsl
#include "ModelCommon.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

cbuffer CbMesh : register(b0)
{
    float4 baseColor;
    
    float3 emissiveColor;
    float metalnessParam;
    float roughnessParam;
    float occlusionStrength;
    float toonPbrBlend;
    float normalScale;
    
    float4 shadeFeathers;
    float4 shadeSteps;
    float4 styleParams;
    float specularStrength;
    float rimStrength;
    float2 stylePadding;
}

Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);

// 半透明モデルも、不透明モデルと同じ進行度で消滅・形成する。
cbuffer CbModelDissolve : register(b4)
{
    float4 dissolveParams;
    float4 dissolveEdgeColor;
    float4 dissolveDirection;
    float4 dissolveOrigin;
};
Texture2D DissolveNoiseMap : register(t18);

TextureCube diffuse_iem : register(t10);
TextureCube specular_pmrem : register(t11);
Texture2D lut_ggx : register(t12);

SamplerState LinearSampler : register(s0);

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float ToonRamp(float value)
{
    float toonAmount = 0.0f;
    
    // それぞれのステップ（閾値）に対して、対応する個別のフェザー（ボカシ幅）を適用します
    toonAmount += smoothstep(shadeSteps.x - shadeFeathers.x, shadeSteps.x + shadeFeathers.x, value);
    toonAmount += smoothstep(shadeSteps.y - shadeFeathers.y, shadeSteps.y + shadeFeathers.y, value);
    toonAmount += smoothstep(shadeSteps.z - shadeFeathers.z, shadeSteps.z + shadeFeathers.z, value);
    toonAmount += smoothstep(shadeSteps.w - shadeFeathers.w, shadeSteps.w + shadeFeathers.w, value);
    
    return toonAmount * 0.25f;
}

float3 CalcFresnelRoughness(float3 f0, float NdotV, float roughness)
{
    return f0 + (max((float3) (1.0f - roughness), f0) - f0)
               * pow(saturate(1.0f - NdotV), 5.0f);
}

float3 DiffuseIBL(float3 N, float3 V, float roughness,
                  float3 diffuseReflectance, float3 f0)
{
    float NdotV = max(0.0001f, dot(N, V));
    float3 kD = 1.0f - CalcFresnelRoughness(f0, NdotV, roughness);
    float3 irrad = diffuse_iem.Sample(LinearSampler, N).rgb;
    return diffuseReflectance * irrad * kD;
}

float3 SpecularIBL(float3 N, float3 V, float roughness, float3 f0)
{
    float NdotV = max(0.0001f, dot(N, V));
    float3 R = normalize(reflect(-V, N));

    uint width, height, mipMaps;
    specular_pmrem.GetDimensions(0, width, height, mipMaps);
    float lod = roughness * float(mipMaps - 1);
    float3 specLight = specular_pmrem.SampleLevel(LinearSampler, R, lod).rgb;

    float2 brdfUV = saturate(float2(NdotV, roughness));
    float2 envBRDF = lut_ggx.Sample(LinearSampler, brdfUV).rg;

    return specLight * (f0 * envBRDF.x + envBRDF.y);
}

float3 ToonDiffuseColor(float3 base, float toonAmount)
{
    float3 shadowColor = base * styleParams.rgb;
    return lerp(shadowColor, base, toonAmount);
}

float3 DirectToonPBR(float3 base, float3 N, float3 V, float3 L, float3 light, float metalness, float roughness, float3 f0)
{
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotV = abs(dot(N, V)) + 1e-5f;
    float NdotH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float toonLight = ToonRamp(NdotL);
    float alphaRoughness = max(roughness * roughness, 0.001f);
    float3 f90 = float3(1.0f, 1.0f, 1.0f);

    float3 toonDiffuse = ToonDiffuseColor(base, toonLight) * (1.0f - metalness);
    //float3 spec = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
    
    float3 pbrDiffuse = brdf_lambertian(f0, f90, base, VoH) * (1.0f - metalness);
    float3 pbrSpec = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
    float3 toonSpec = pbrSpec;


    float specMask = smoothstep(0.25f, 0.75f, saturate(max(toonSpec.r, max(toonSpec.g, toonSpec.b)) * (1.0f - roughness)));
    toonSpec *= lerp(0.35f, 1.0f, specMask);
 
    // 拡散光だけをセル調へ寄せ、反射は独立したToon化率で混ぜる。
    float3 diffuseResult_ = lerp(pbrDiffuse * NdotL, toonDiffuse * toonLight, saturate(toonPbrBlend));
    float3 specularResult_ = lerp(pbrSpec * NdotL, toonSpec * NdotL, saturate(styleParams.w));
    return (diffuseResult_ + specularResult_ * max(specularStrength, 0.0f)) * light;
}

PS_OUT main(VS_OUT pin)
{
    float dissolveEdge_ = 0.0f;
    if (dissolveParams.x > 0.0f)
    {
        const float noise_ = dissolveOrigin.w > 0.5f
            ? DissolveNoiseMap.Sample(LinearSampler, pin.texcoord * dissolveParams.w).r
            : frac(sin(dot(floor(pin.texcoord * dissolveParams.w * 64.0f), float2(12.9898f, 78.233f))) * 43758.5453f);
        const float direction_ = saturate(dot(pin.position - dissolveOrigin.xyz, dissolveDirection.xyz));
        const float field_ = lerp(noise_, direction_, dissolveDirection.w);
        const float threshold_ = dissolveParams.x * (1.0f + dissolveParams.y);
        clip(field_ - threshold_);
        dissolveEdge_ = 1.0f - saturate((field_ - threshold_) / max(dissolveParams.y, 0.001f));
    }
    float4 base = BaseMap.Sample(LinearSampler, pin.texcoord) * baseColor;
    if (base.a < 0.1f)
        discard;
  
    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent.xyz);

    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T)) * pin.tangent.w;

    float3x3 TBN = float3x3(T, B, N);

    float3 normalSample = NormalMap.Sample(LinearSampler, pin.texcoord).xyz * 2.0f - 1.0f;
    normalSample.xy *= normalScale;

    N = normalize(mul(normalSample, TBN));
    float3 V = normalize(cameraPosition.xyz - pin.position);

    float4 metalRoughSample = MetalRoughMap.Sample(LinearSampler, pin.texcoord);
    float2 materialValues_ = ApplySeparateMaterialMaps(
        metalRoughSample, float2(metalnessParam, roughnessParam), LinearSampler, pin.texcoord);
    float metalness = saturate(materialValues_.r);
    float roughness = saturate(materialValues_.g);
    roughness = max(roughness, 0.04f);

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base.rgb, metalness);
    float3 diffuseReflectance = base.rgb * (1.0f - metalness);

    float3 totalDirectLighting = 0.0f;
    {
        float3 L = normalize(-lightDirection.xyz);
        float directionalIntensity = lightDirection.w;
        float shadow = CalcCascadeShadow(pin.position);
        totalDirectLighting += DirectToonPBR(
            base.rgb, N, V, L,
            lightColor.rgb * directionalIntensity * shadow,
            metalness, roughness, f0);
    }

    [loop]
    for (int i = 0; i < numPointLights; i++)
    {
        PointLightData pl = pointLights[i];
        float3 Lvec = pl.position - pin.position;
        float distanceSquared = dot(Lvec, Lvec);
        float rangeSquared = pl.range * pl.range;
        if (distanceSquared > rangeSquared)
            continue;

        float inverseDistance = rsqrt(max(distanceSquared, 0.000001f));
        float d = distanceSquared * inverseDistance;
        float att = saturate(1.0f - (d / pl.range));
        att *= att;

        totalDirectLighting += DirectToonPBR(
            base.rgb, N, V, Lvec * inverseDistance,
            pl.color * pl.intensity * att,
            metalness, roughness, f0);
    }

    [loop]
    for (int j = 0; j < numSpotLights; j++)
    {
        SpotLightData sl = spotLights[j];
        float3 Lvec = sl.position - pin.position;
        float distanceSquared = dot(Lvec, Lvec);
        float rangeSquared = sl.range * sl.range;
        if (distanceSquared > rangeSquared)
            continue;

        float inverseDistance = rsqrt(max(distanceSquared, 0.000001f));
        float d = distanceSquared * inverseDistance;
        float3 L = Lvec * inverseDistance;
        float cosAngle = dot(normalize(sl.direction), -L);
        float cosInner = cos(radians(sl.innerAngle));
        float cosOuter = cos(radians(sl.outerAngle));
        float spotAtt = saturate((cosAngle - cosOuter) / max(cosInner - cosOuter, 1e-4f));
        spotAtt *= spotAtt;
        if (spotAtt <= 0.0f)
            continue;

        float att = saturate(1.0f - (d / sl.range));
        att = att * att * spotAtt;

        totalDirectLighting += DirectToonPBR(
            base.rgb, N, V, L,
            sl.color * sl.intensity * att,
            metalness, roughness, f0);
    }

    float3 iblDiffuse = DiffuseIBL(N, V, roughness, diffuseReflectance, f0);
    float3 iblSpecular = SpecularIBL(N, V, roughness, f0);

    float aoSample = AOMap.Sample(LinearSampler, pin.texcoord).r;
    float ao = lerp(1.0f, aoSample, occlusionStrength);

    float3 emissive = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveColor.rgb;
    emissive += dissolveEdgeColor.rgb * dissolveEdge_ * dissolveParams.z;

    // Toonでも反射を残し、金属や濡れた材質の立体感を補う。
    float iblDiffuseStrength = lerp(1.0f, 0.4f, toonPbrBlend);
    float iblSpecularStrength = lerp(1.0f, 0.35f, saturate(styleParams.w)) * max(specularStrength, 0.0f);

    float3 finalColor =
    totalDirectLighting +
    iblDiffuse * ao * iblDiffuseStrength +
    iblSpecular * iblSpecularStrength +
    emissive;
    
    // Bloomと半透明合成へ線形HDRを渡し、表示変換は最終合成で行う。
    finalColor = max(finalColor, 0.0f);
    // 暗い背景でも輪郭を読めるよう、直接光とは別に弱いRimを加える。
    float rim_ = pow(1.0f - saturate(abs(dot(N, V))), 3.0f) * max(rimStrength, 0.0f);
    finalColor += rim_ * lerp(base.rgb, float3(1.0f, 1.0f, 1.0f), 0.5f);


    // PostEffectのないEditorプレビューだけ、ここで表示用の色へ変換する。
    if (outputLinearColor < 0.5f)
    {
        finalColor = pow(max(ACESFilm(finalColor), 0.0f), 1.0f / 2.2f);
    }

    PS_OUT output;
    output.color = float4(finalColor, base.a);
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    return output;
}
