// PBRPS.hlsl  ---- IBL 追加版 ----
#include "PBRCommon.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

static const float PI = 3.14159265f;

// -----------------------------------------------
// メッシュ定数バッファ (b0)
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
// ACES トーンマッピング関数
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
// テクスチャ (t0t4 : 既存、t10t12 : IBL 追加)
// -----------------------------------------------
Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);

// IBL テクスチャ
// ※ register 番号は GraphicsDeviceManager 側と必ず合わせること
TextureCube diffuse_iem : register(t10); // 拡散反射用 IrradianceMap
TextureCube specular_pmrem : register(t11); // 鏡面反射用 RadianceMap
Texture2D lut_ggx : register(t12); // GGX BRDF 参照テーブル

// -----------------------------------------------
// サンプラー
// -----------------------------------------------
SamplerState LinearSampler : register(s0);


// -----------------------------------------------
// IBL ヘルパー関数
// -----------------------------------------------

// 粗さを考慮したフレネル近似
float3 CalcFresnelRoughness(float3 f0, float NdotV, float roughness)
{
    return f0 + (max((float3) (1.0f - roughness), f0) - f0)
               * pow(saturate(1.0f - NdotV), 5.0f);
}

// 拡散反射 IBL
float3 DiffuseIBL(float3 N, float3 V, float roughness,
                  float3 diffuse_reflectance, float3 f0)
{
    float NdotV = max(0.0001f, dot(N, V));
    float3 kD = 1.0f - CalcFresnelRoughness(f0, NdotV, roughness);
    float3 irrad = diffuse_iem.Sample(LinearSampler, N).rgb;
    return diffuse_reflectance * irrad * kD;
}

// 鏡面反射 IBL (Split Sum Approximation)
float3 SpecularIBL(float3 N, float3 V, float roughness, float3 f0)
{
    float NdotV = max(0.0001f, dot(N, V));
    float3 R = normalize(reflect(-V, N));

    // roughness をミップレベルに変換してサンプリング
    uint width, height, mipMaps;
    specular_pmrem.GetDimensions(0, width, height, mipMaps);
    float lod = roughness * float(mipMaps - 1);
    float3 specLight = specular_pmrem.SampleLevel(LinearSampler, R, lod).rgb;

    // BRDF LUT から F_scale / F_bias を取得
    float2 brdfUV = saturate(float2(NdotV, roughness));
    float2 envBRDF = lut_ggx.Sample(LinearSampler, brdfUV).rg;

    return specLight * (f0 * envBRDF.x + envBRDF.y);
}

// -----------------------------------------------
// メイン
// -----------------------------------------------
PS_OUT main(VS_OUT pin)
{
    // --- 1. テクスチャサンプリング ---
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

    // --- 2. PBR パラメータ ---
    float2 metalRoughSample = MetalRoughMap.Sample(LinearSampler, pin.texcoord).gr;
    float metalness = metalRoughSample.r * metalnessParam;
    float roughness = metalRoughSample.g * roughnessParam;
    float alphaRoughness = roughness * roughness;

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base.rgb, metalness);
    float3 f90 = float3(1.0f, 1.0f, 1.0f);

    // 拡散反射率（金属は 0）
    float3 diffuse_reflectance = base.rgb * (1.0f - metalness);

    // --- 3. 直接光ライティング ---
    float3 totalDirectLighting = 0;

    // A. 平行光源
    {
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotV = abs(dot(N, V)) + 1e-5f;
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float3 spec = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diff = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0f - metalness);

        float directionalIntensity = lightColor.w;
        totalDirectLighting += (diff + spec) * (lightColor.rgb * directionalIntensity) * NdotL;
    }

    // B. 点光源
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

    // C. スポットライト
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

  // IBLの計算
    float3 iblDiffuse = DiffuseIBL(N, V, roughness, diffuse_reflectance, f0);
    float3 iblSpecular = SpecularIBL(N, V, roughness, f0);

    // AO / Emissive
    float ao = AOMap.Sample(LinearSampler, pin.texcoord).r * occlusionStrength;
    float3 emissive = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveColor.rgb;

    // --- 改良ポイント：AOは拡散反射IBLにのみ適用 ---
    float3 finalColor = (totalDirectLighting + (iblDiffuse * ao) + iblSpecular) + emissive;

    // --- 改良ポイント：トーンマッピングの適用 ---
    finalColor = ACESFilm(finalColor);

    // ガンマ補正
    finalColor = pow(max(finalColor, 0.0f), 1.0f / 2.2f);

    PS_OUT output; 
    // カラーの出力
    output.color = float4(finalColor, base.a);   
    // 法線の出力（法線マップ適用後のベクトルを [0, 1] に正規化）
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    // 深度の出力（カメラからの距離）
    // 線形深度を算出する（near/far等で正規化する場合はここを調整せよ）
    output.depth = length(pin.position - cameraPosition.xyz);
    
    return output;
}