#include "PBRCommon.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

static const float PI = 3.14159265f;

cbuffer CbMesh : register(b0)
{
    float4 baseColor;
    float3 emissiveColor;
    float metalnessParam;
    float roughnessParam;
    float occlusionStrength;
    float padding[2]; //  8バイト (40～47) -> 全体を16の倍数にする
};

Texture2D BaseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);

SamplerState LinearSampler : register(s0);

// ★ 戻り値用の構造体を定義
struct PS_OUT
{
    float4 color : SV_TARGET0; // シーンカラー用 (sceneRTV)
    float4 normal : SV_TARGET1; // 法線バッファ用 (sceneNormalRTV)
};

PS_OUT main(VS_OUT pin) : SV_TARGET
{
    // --- 1. サンプリングと基本ベクトル ---
    float4 base = BaseMap.Sample(LinearSampler, pin.texcoord) * baseColor * 0.8;
    if (base.a < 0.1)
        discard; // アルファテスト（必要に応じて）

    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    float3 normalSample = NormalMap.Sample(LinearSampler, pin.texcoord).xyz * 2.0 - 1.0;
    N = normalize(mul(normalSample, TBN));

    float3 L = normalize(-lightDirection.xyz);
    float3 V = normalize(cameraPosition.xyz - pin.position);
    float3 H = normalize(L + V);

    // ドット積
    float NdotL = saturate(dot(N, L));
    float NdotV = abs(dot(N, V)) + 1e-5f; // 0除算防止
    float NdotH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    // --- 2. PBRパラメータの準備 ---
    float2 metalRoughSample = MetalRoughMap.Sample(LinearSampler, pin.texcoord).rg;
    float metalness = metalRoughSample.r * metalnessParam;
    float roughness = metalRoughSample.g * roughnessParam;
    
    // 物理ベースの荒さは roughness^2 を alpha とする
    float alphaRoughness = roughness * roughness;

    // F0の計算: 非金属は 0.04 固定、金属は baseColor を使用
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), base.rgb, metalness);
    float3 f90 = float3(1.0, 1.0, 1.0);

    // --- 3. BRDFの計算 (ヘッダー関数の使用) ---
    
    // スペキュラ項 (D * G * F / (4*NoL*NoV) が関数内で完結)
    float3 specularBRDF = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
    
    // ディフューズ項 (1-F を考慮した Lambert)
    float3 diffuseBRDF = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0 - metalness);

    // --- 3. ライティング計算 ---
    float3 totalDirectLighting = float3(0, 0, 0);
    
    // ==========================================
    // A. 平行光源 (Directional Light)
    // ==========================================
{
        float3 L = normalize(-lightDirection.xyz);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float3 specularBRDF = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuseBRDF = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0 - metalness);

        // lightColor.w を強度として掛ける
        float directionalIntensity = lightColor.w;
        totalDirectLighting += (diffuseBRDF + specularBRDF) * (lightColor.rgb * directionalIntensity) * NdotL;
    }

  // ==========================================
    // B. 複数の点光源 (Point Lights Loop)
    // ==========================================
    [loop]
    for (int i = 0; i < numPointLights; i++)
    {
        PointLightData pl = pointLights[i];
        
        float3 Lvec = pl.position - pin.position;
        float d = length(Lvec);
        
        // 範囲外なら計算をスキップ
        if (d > pl.range)
            continue;

        float3 L = normalize(Lvec);
        float3 H = normalize(L + V);
        
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        // 減衰計算
        float attenuation = saturate(1.0 - (d / pl.range));
        attenuation *= attenuation;

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0 - metalness);

        float3 pColor = pl.color * pl.intensity;
        totalDirectLighting += (diffuse + specular) * pColor * NdotL * attenuation;
    }
    
    [loop]
    for (int j = 0; j < numSpotLights; j++)
    {
        SpotLightData sl = spotLights[j];
    
        float3 Lvec = sl.position - pin.position;
        float d = length(Lvec);
    
    // 1. 距離による減衰 (点光源と同じ)
        if (d > sl.range)
            continue;
        float3 L = normalize(Lvec);
    
    // 2. 角度による減衰
    // ライトの向き(sl.direction)と、ピクセルへの方向(-L)の内積を計算
        float cosAngle = dot(normalize(sl.direction), -L);
    
    // 角度をコサイン値に変換（C++側で計算済みなら不要。度数法で来る想定）
        float cosInner = cos(radians(sl.innerAngle));
        float cosOuter = cos(radians(sl.outerAngle));
    
    // 内角と外角の間で 0.0 ～ 1.0 に補間
        float spotAttenuation = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));
    // お好みで滑らかに
        spotAttenuation *= spotAttenuation;

        if (spotAttenuation <= 0.0f)
            continue;

    // 3. PBRライティング計算
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        float attenuation = saturate(1.0 - (d / sl.range));
        attenuation *= attenuation;
        attenuation *= spotAttenuation; // スポット減衰を乗算

        float3 specular = brdf_specular_ggx(f0, f90, alphaRoughness, VoH, NdotL, NdotV, NdotH);
        float3 diffuse = brdf_lambertian(f0, f90, base.rgb, VoH) * (1.0 - metalness);

        float3 sColor = sl.color * sl.intensity;
        totalDirectLighting += (diffuse + specular) * sColor * NdotL * attenuation;
    }
    
// --- 4. 環境光・AO・発光 ---
    float ao = AOMap.Sample(LinearSampler, pin.texcoord).r * occlusionStrength;

    // 修正：emissiveColor を単なる色ではなく「色 × 強度」として扱う
    // ※ C++側のGUIで emissiveColor を (10, 10, 10) などに設定できるようにすると激しく光ります
    float3 emissive = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveColor.rgb;

    // --- 5. 最終カラー合成 ---
    float3 ambientColor = float3(0.03, 0.03, 0.03) * base.rgb * ao;
    float3 finalColor = totalDirectLighting * ao + ambientColor + emissive;

    // --- 6. ポストプロセス ---
    finalColor = pow(finalColor, 1.0 / 1.25);

    // --- 7. 【重要】MRT出力 ---
    PS_OUT output;
    
    // SV_TARGET0: 最終的な色
    output.color = float4(finalColor, base.a);
    
    // SV_TARGET1: ワールド法線を保存
    // 範囲を [-1, 1] から [0, 1] に圧縮して保存します
    // これによりデカールシェーダー側で正確な面の向きが取得可能になります
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    
    return output;
}