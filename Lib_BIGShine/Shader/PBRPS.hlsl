// PBRPS.hlsl  ---- IBL ????? ----
#include "ModelCommon.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

static const float PI = 3.14159265f;

// -----------------------------------------------
// ???b?V?????o?b?t?@ (b0)
// -----------------------------------------------
cbuffer CbMesh : register(b0)
{
    float4 baseColor;
    float3 emissiveColor;
    float metalnessParam;
    float roughnessParam;
    float occlusionStrength;
    float padding[2];
};

// -----------------------------------------------
// ACES ?g?[???}?b?s???O???
// -----------------------------------------------
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// -----------------------------------------------
// ?e?N?X?`?? (t0t4 : ?????At10t12 : IBL ???)
// -----------------------------------------------
Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);

// IBL ?e?N?X?`??
// ?? register ????? GraphicsDeviceManager ????K???????Á∑??
TextureCube diffuse_iem : register(t10); // ?g?U????p IrradianceMap
TextureCube specular_pmrem : register(t11); // ???????p RadianceMap
Texture2D lut_ggx : register(t12); // GGX BRDF ?Q??e?[?u??

// -----------------------------------------------
// ?T???v???[
// -----------------------------------------------
SamplerState LinearSampler : register(s0);


// -----------------------------------------------
// IBL ?w???p?[???
// -----------------------------------------------

// ?e????l???????t???l?????
float3 CalcFresnelRoughness(float3 f0, float NdotV, float roughness)
{
    return f0 + (max((float3) (1.0f - roughness), f0) - f0)
               * pow(saturate(1.0f - NdotV), 5.0f);
}

// ?g?U???? IBL
float3 DiffuseIBL(float3 N, float3 V, float roughness,
                  float3 diffuse_reflectance, float3 f0)
{
    float NdotV = max(0.0001f, dot(N, V));
    float3 kD = 1.0f - CalcFresnelRoughness(f0, NdotV, roughness);
    float3 irrad = diffuse_iem.Sample(LinearSampler, N).rgb;
    return diffuse_reflectance * irrad * kD;
}

// ??????? IBL (Split Sum Approximation)
float3 SpecularIBL(float3 N, float3 V, float roughness, float3 f0)
{
    float NdotV = max(0.0001f, dot(N, V));
    float3 R = normalize(reflect(-V, N));

    // roughness ??~?b?v???x??????????T???v?????O
    uint width, height, mipMaps;
    specular_pmrem.GetDimensions(0, width, height, mipMaps);
    float lod = roughness * float(mipMaps - 1);
    float3 specLight = specular_pmrem.SampleLevel(LinearSampler, R, lod).rgb;

    // BRDF LUT ???? F_scale / F_bias ??èÔ
    float2 brdfUV = saturate(float2(NdotV, roughness));
    float2 envBRDF = lut_ggx.Sample(LinearSampler, brdfUV).rg;

    return specLight * (f0 * envBRDF.x + envBRDF.y);
}

// -----------------------------------------------
// ???C??
// -----------------------------------------------
PS_OUT main(VS_OUT pin)
{
    // --- 1. ?e?N?X?`???T???v?????O ---
    float4 base = BaseMap.Sample(LinearSampler, pin.texcoord) * baseColor * 0.8f;
    if (base.a < 0.1f)
        discard;

    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    float3 normalSample = NormalMap.Sample(LinearSampler, pin.texcoord).xyz * 2.0f - 1.0f;
    N = normalize(mul(normalSample, TBN));

    float3 V = normalize(cameraPosition.xyz - pin.position);

    // --- 2. PBR ?p?????[?^ ---
    float2 metalRoughSample = MetalRoughMap.Sample(LinearSampler, pin.texcoord).gr;
    float metalness = metalRoughSample.r * metalnessParam;
    float roughness = metalRoughSample.g * roughnessParam;
    float alphaRoughness = roughness * roughness;

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base.rgb, metalness);
    float3 f90 = float3(1.0f, 1.0f, 1.0f);

    // ?g?U??????i????? 0?j
    float3 diffuse_reflectance = base.rgb * (1.0f - metalness);

    // --- 3. ????????C?e?B???O ---
    float3 totalDirectLighting = 0;

    // A. ???s????
    {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotV = abs(dot(N, V)) + 1e-5f;
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float3 spec = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diff = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0f - metalness);

        float directionalIntensity = lightDirection.w;
        float shadow = CalcCascadeShadow(pin.position);
        totalDirectLighting += (diff + spec) * (lightColor.rgb * directionalIntensity) * NdotL * shadow;
    }

    // B. ?_????
    [loop]
    for (int i = 0; i < numPointLights; i++)
    {
        PointLightData pl = pointLights[i];
        float3 Lvec = pl.position - pin.position;
        float d = length(Lvec);
        if (d > pl.range)
            continue;

        float3 L = normalize(Lvec);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotV = abs(dot(N, V)) + 1e-5f;
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float att = saturate(1.0f - (d / pl.range));
        att *= att;

        float3 spec = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diff = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0f - metalness);

        totalDirectLighting += (diff + spec) * (pl.color * pl.intensity) * NdotL * att;
    }

    // C. ?X?|?b?g???C?g
    [loop]
    for (int j = 0; j < numSpotLights; j++)
    {
        SpotLightData sl = spotLights[j];
        float3 Lvec = sl.position - pin.position;
        float d = length(Lvec);
        if (d > sl.range)
            continue;

        float3 L = normalize(Lvec);
        float cosAngle = dot(normalize(sl.direction), -L);
        float cosInner = cos(radians(sl.innerAngle));
        float cosOuter = cos(radians(sl.outerAngle));
        float spotAtt = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));
        spotAtt *= spotAtt;
        if (spotAtt <= 0.0f)
            continue;

        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotV = abs(dot(N, V)) + 1e-5f;
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float att = saturate(1.0f - (d / sl.range));
        att = att * att * spotAtt;

        float3 spec = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diff = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0f - metalness);

        totalDirectLighting += (diff + spec) * (sl.color * sl.intensity) * NdotL * att;
    }

  // IBL??v?Z
    float3 iblDiffuse = DiffuseIBL(N, V, roughness, diffuse_reflectance, f0);
    float3 iblSpecular = SpecularIBL(N, V, roughness, f0);

    // AO / Emissive
    float ao = AOMap.Sample(LinearSampler, pin.texcoord).r * occlusionStrength;
    float3 emissive = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveColor.rgb;

    // --- AO??g?U????IBL????K?p ---
    float3 finalColor = (totalDirectLighting + (iblDiffuse * ao) + iblSpecular) + emissive;

    // --- ?g?[???}?b?s???O??K?p ---
    finalColor = ACESFilm(finalColor);

    // ?K???}??
    finalColor = pow(max(finalColor, 0.0f), 1.0f / 2.2f);

    PS_OUT output; 
    // ?J???[??o??
    output.color = float4(finalColor, base.a);   
    // ?@????o??i?@???}?b?v?K?p???x?N?g???? [0, 1] ????K???j
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    return output;
}