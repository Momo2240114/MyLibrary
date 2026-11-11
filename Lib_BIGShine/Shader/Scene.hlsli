#ifndef SCENE_HLSLI // もし定義されていなければ…
#define SCENE_HLSLI // 定義する

cbuffer CbScene : register(b7)
{
    row_major float4x4 viewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    
    float4 z_buffer_parameteres;
    
    // 点光源管理用
    int numPointLights; // 現在シーンにある有効なポイントライト数
    int numSpotLights; // 現在シーンにある有効なスポットライト数
    // PostEffectを通すシーンと、直接表示するEditorプレビューを区別する。
    float outputLinearColor;
    float scenePadding; // 16バイト境界合わせ
    
    row_major float4x4 invViewProjection;
};

// --- 構造体定義 (C++のPointLightDataと一致させる) ---
struct PointLightData
{
    float3 position;
    float range;
    float3 color;
    float intensity;
};

struct SpotLightData
{
    float3 position;
    float range;
    float3 direction;
    float intensity;
    float3 color;
    float innerAngle; // 度数法、またはラジアン（今回は計算内でcosに変換）
    float outerAngle;
    float padding;
};

// --- 構造化バッファ (t5) ---
StructuredBuffer<PointLightData> pointLights : register(t5);
StructuredBuffer<SpotLightData> spotLights : register(t6); // 追加

#endif // SCENE_HLSLI の終わり