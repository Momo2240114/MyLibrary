// ToonPS.hlsl
#include "Scene.hlsli" // これでCbSceneやStructuredBufferが使えるようになる
#include "ModelCommon.hlsli"


cbuffer CbMesh : register(b0)
{
    float4 baseColor;
    float3 emissiveColor;
    float metalness;
    float roughness;
    float shadeStep;
    float shadeFeather;
    float occlusionStrength;
    float padding;
}

Texture2D baseMap : register(t0);

Texture2D NormalMap : register(t1);
Texture2D EmissiveMap : register(t2);
Texture2D AOMap : register(t3);
Texture2D MetalRoughMap : register(t4);

SamplerState linearSampler : register(s0);

// アニメ調の陰影を計算するための関数（彩度と明度を考慮）
float3 CalculateToonDiffuse(float3 albedo, float NdotL, float3 lightCol)
{
    // 影の境界線をパキッとさせる
    float toonShade = smoothstep(shadeStep - shadeFeather, shadeStep + shadeFeather, NdotL);
    
    // 影の部分には少し彩度を乗せ、寒色寄りにするとアニメらしい深みが出る
    float3 shadowColor = albedo * 0.5f;
    shadowColor *= float3(0.8f, 0.9f, 1.0f); // 影に少し青みを加える
    
    return lerp(shadowColor, albedo, toonShade) * lightCol;
}

PS_OUT main(VS_OUT pin) : SV_TARGET
{
    float4 albedo = baseMap.Sample(linearSampler, pin.texcoord) * baseColor;
    float3 N = normalize(pin.normal);
    float3 V = normalize(cameraPosition.xyz - pin.position);
    float3 L_dir = normalize(lightDirection.xyz);

    // 1. 拡散反射（トゥーン化）
    float NdotL = saturate(dot(N, L_dir));
    float3 diffuse = CalculateToonDiffuse(albedo.rgb, NdotL, lightColor.rgb);

    // 2. スペキュラ（アニメ調のハイライト）
    // 鏡面反射を極端に絞り込み、面積を小さくする
    float3 H = normalize(L_dir + V);
    float NdotH = saturate(dot(N, H));
    float specIntensity = pow(NdotH, 100.0f); // 指数を高めて小さく鋭い光に
    float3 specular = step(0.95f, specIntensity) * lightColor.rgb; // パキッとしたハイライト

    // 最終カラーの決定
    float3 finalColor = diffuse + specular;

    // 出力の生成
    PS_OUT pout;
    pout.color = float4(finalColor, albedo.a);
    
    // 法線情報を [-1, 1] から [0, 1] に変換して格納（テクスチャバッファ用）
    pout.normal = float4(normalize(pin.normal) * 0.5f + 0.5f, 1.0f);
    
    // 深度情報（カメラからの距離）
    // 線形深度を求めるのが一般的だ。ピンのワールド座標からカメラ位置を引き、長さを算出する
    float depth = length(pin.position - cameraPosition.xyz);
    pout.depth = depth; // 必要に応じて near/far で正規化せよ

    return pout;
}