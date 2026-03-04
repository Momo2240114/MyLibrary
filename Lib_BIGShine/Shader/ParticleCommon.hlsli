// ParticleCommon.hlsli

#include "Scene.hlsli"

#define PI 3.1415926535f
#define NUMTHREADS_X 16
#define STATE_DEAD 0
#define STATE_ALIVE 1

// 1粒あたりのデータ（C++側と完全に一致させる）
struct Particle
{
    float3 position; // 12
    float age; // 4 (合計16)
    
    float3 velocity; // 12
    float lifeSpan; // 4 (合計16)
    
    float3 acceleration; // 12
    float rotation; // 4 (合計16)
    
    float4 startColor; // 16
    float4 endColor; // 16
    
    float2 size; // 8
    uint type; // 4
    uint isActive; // 4 (合計16)
};

// 定数バッファ (b9)
cbuffer ParticleSystemConstants : register(b9)
{
    float4 emissionPosition;
    float2 emissionOffset;
    float2 emissionSize;
    float2 emissionConeAngle;
    float2 emissionSpeed;
    float2 emissionAngularSpeed;
    float2 lifespan;
    float2 spawnDelay;
    float2 fadeDuration;
    float4 manualColor;
    float time;
    float deltaTime;
    float noiseScale;
    float gravity;
    uint maxParticleCount;
    uint particleRenderType;
    uint emissionMode;
    uint burstCount;
    float burstTrigger;
    float3 padding;
};

// お手本流：ランダム数値生成
float rand(uint seed)
{
    return frac(sin(seed * 12.9898) * 43758.5453);
}

// ジオメトリシェーダーからピクセルシェーダーへ渡す構造体
struct GS_OUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 tex : TEXCOORD0;
    float3 worldPos : TEXCOORD1; // ワールド座標を渡すための変数
    uint type : BLENDWEIGHT0; 
};

// 頂点シェーダーからジオメトリシェーダーへ渡す構造体
struct VS_OUT
{
    uint vertexID : TEXCOORD0;
};