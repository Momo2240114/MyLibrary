// ToonPS.hlsl
#include "Scene.hlsli" // これでCbSceneやStructuredBufferが使えるようになる
#include "ModelCommon.hlsli"


cbuffer CbMesh : register(b0)
{
    float4 baseColor;
    float3 emissiveColor;
    float metalness;
    float roughness;

    float occlusionStrength;
    float4 shadeSteps;
    float4 shadeFeathers;
    float2 padding;
}

Texture2D baseMap : register(t0);

Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);

//  IBL用のテクスチャとサンプラーを登録！
TextureCube diffuse_iem : register(t10);
TextureCube specular_pmrem : register(t11); // もし金属光沢風のハイライトも入れたければ
Texture2D lut_ggx : register(t12);
SamplerState linearSampler : register(s0);


//{
//    // 影の境界線をパキッとさせる
//    float toonShade = smoothstep(shadeStep - shadeFeather, shadeStep + shadeFeather, NdotL);
    
//    // 影の部分には少し彩度を乗せ、寒色寄りにするとアニメらしい深みが出る
//    float3 shadowColor = albedo * 0.5f;
//    shadowColor *= float3(0.8f, 0.9f, 1.0f); // 影に少し青みを加える
    
//    return lerp(shadowColor, albedo, toonShade) * lightCol;
//}

// トーンマッピング関数（PBR側と画面のトーンを合わせるために統一）
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 影の境界線を制御するToonの命
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

// アニメ調の陰影色を決定する（1号影を想定。少し青みを含ませて美しくする）
float3 CalculateToonDiffuse(float3 albedo, float toonAmount)
{
    float3 shadowColor = albedo * 0.45f;
    shadowColor *= float3(0.85f, 0.92f, 1.0f); // 影に透明感を与える寒色補正
    return lerp(shadowColor, albedo, toonAmount);
}

// すべての個別ライト（平行・点・スポット）からピュアなToonライティングを導く関数
float3 DirectToonLighting(float3 base, float3 N, float3 V, float3 L, float3 lightColorIntensity)
{
    float NdotL = saturate(dot(N, L));
    float toonLight = ToonRamp(NdotL);

    // 1. 拡散反射（ToonDiffuse）
    float3 diffuseResult = CalculateToonDiffuse(base, toonLight) * lightColorIntensity * toonLight;

    // 2. 鏡面反射（アニメ調のパキッとしたシャープなハイライト）
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float specIntensity = pow(NdotH, 120.0f); // 非常に鋭いハイライト
    float3 toonSpec = (specIntensity > 0.4f ? 1.0f : 0.0f) * lightColorIntensity * 0.5f;

    return diffuseResult + toonSpec;
}

PS_OUT main(VS_OUT pin)
{
    float4 base = baseMap.Sample(linearSampler, pin.texcoord) * baseColor;
    if (base.a < 0.1f)
        discard;

    // 法線マップによるディテール展開
    float3 N = normalize(pin.normal);
    float3 T = normalize(pin.tangent);
    float3 B = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    float3 normalSample = NormalMap.Sample(linearSampler, pin.texcoord).xyz * 2.0f - 1.0f;
    N = normalize(mul(normalSample, TBN));

    float3 V = normalize(cameraPosition.xyz - pin.position);

    // パラメータサンプリング（ToonPBRとのデータ互換を維持）
    float2 metalRoughSample = MetalRoughMap.Sample(linearSampler, pin.texcoord).gr;
    float roughnessParm = saturate(metalRoughSample.g * roughness);
    roughnessParm = max(roughnessParm, 0.04f);

    // 蓄積されるダイレクトライトの器
    float3 totalDirectLighting = 0.0f;

    // =============================================================
    // 1. 平行光源（主光源 ＋ キャスケードシャドウ）
    // =============================================================
    {
        float3 L = normalize(-lightDirection.xyz);
        float directionalIntensity = lightDirection.w;
        float shadow = CalcCascadeShadow(pin.position);

        totalDirectLighting += DirectToonLighting(
            base.rgb, N, V, L,
            lightColor.rgb * directionalIntensity * shadow
        );
    }

    // =============================================================
    // 2. 点光源（Point Lights）ループ
    // =============================================================
    [loop]
    for (int i = 0; i < numPointLights; i++)
    {
        PointLightData pl = pointLights[i];
        float3 Lvec = pl.position - pin.position;
        float d = length(Lvec);
        if (d > pl.range)
            continue;

        float att = saturate(1.0f - (d / pl.range));
        att *= att;

        totalDirectLighting += DirectToonLighting(
            base.rgb, N, V, normalize(Lvec),
            pl.color * pl.intensity * att
        );
    }

    // =============================================================
    // 3. スポットライト（Spot Lights）ループ
    // =============================================================
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
        float spotAtt = saturate((cosAngle - cosOuter) / max(cosInner - cosOuter, 1e-4f));
        spotAtt *= spotAtt;
        if (spotAtt <= 0.0f)
            continue;

        float att = saturate(1.0f - (d / sl.range));
        att = att * att * spotAtt;

        totalDirectLighting += DirectToonLighting(
            base.rgb, N, V, L,
            sl.color * sl.intensity * att
        );
    }

    // =============================================================
    // 4. トゥーン流・環境光（IBL）の調停
    // =============================================================
    // 背景の環境光（拡散成分）をサンプリング
    float3 envIrradiance = diffuse_iem.Sample(linearSampler, N).rgb;
    
    // 主光源の当たり具合（NdotL）をベースに、環境光の「ToonRamp」を計算
    float mainNdotL = saturate(dot(N, normalize(-lightDirection.xyz)));
    float toonEnvMask = ToonRamp(mainNdotL);

    // UTS（UnityChanToonShader）風のIBL制御：
    // 日向（toonEnvMaskが1）では環境光の影響を抑え、影（0）になるほど背景の環境光の色（envIrradiance）が美しく溶け込むようにします。
    float3 iblDiffuse = base.rgb * envIrradiance * lerp(0.6f, 0.2f, toonEnvMask);

    // スペキュラIBL（周囲の景色が髪や金属のハイライトにほんのり映り込む表現）
    float3 R = normalize(reflect(-V, N));
    uint width, height, mipMaps;
    specular_pmrem.GetDimensions(0, width, height, mipMaps);
    float lod = roughnessParm * float(mipMaps - 1);
    float3 envSpecular = specular_pmrem.SampleLevel(linearSampler, R, lod).rgb;
    
    // 映り込みの強さをラフネスで極端にマスクし、アニメのハイライト感を保つ
    float3 iblSpecular = envSpecular * (1.0f - roughnessParm) * 0.1f;

    // 遮蔽（AO）と自己発光（Emissive）の適用
    float aoSample = AOMap.Sample(linearSampler, pin.texcoord).r;
    float ao = lerp(1.0f, aoSample, occlusionStrength);
    float3 emissive = EmissiveMap.Sample(linearSampler, pin.texcoord).rgb * emissiveColor.rgb;

    // =============================================================
    // 5. 最終色の結合とポスト処理
    // =============================================================
    float3 finalColor =
        totalDirectLighting +
        (iblDiffuse * ao) +
        iblSpecular +
        emissive;

    // ポストプロセス（PBR側と完全にトーンを合わせるためのACESToneMapping & Gamma）
    finalColor = ACESFilm(finalColor);
    finalColor = pow(max(finalColor, 0.0f), 1.0f / 2.2f);

    PS_OUT output;
    output.color = float4(finalColor, base.a);
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    output.depth = length(pin.position - cameraPosition.xyz);
    return output;
}