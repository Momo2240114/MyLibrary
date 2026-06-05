#include "Scene.hlsli"
// PBRCommon.hlsli

struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 position : POSITION;
    float3 tangent : TANGENT;
};


// -----------------------------------------------
// MRT ?o??
// -----------------------------------------------
struct PS_OUT
{
    float4 color : SV_TARGET0; // ?V?[???J???[
    float4 normal : SV_TARGET1; // ?@???o?b?t?@
    float depth : SV_TARGET2;
};

static const int CascadeShadowCount = 4;

cbuffer CbCascadeShadow : register(b8)
{
    row_major float4x4 cascadeLightViewProjections[CascadeShadowCount];
    float4 cascadeSplits;
    float4 cascadeShadowBias;
    float cascadeShadowAttenuation;
    float3 cascadeShadowPadding;
};

Texture2D CascadeShadowMap0 : register(t13);
Texture2D CascadeShadowMap1 : register(t14);
Texture2D CascadeShadowMap2 : register(t15);
Texture2D CascadeShadowMap3 : register(t16);
SamplerState ShadowSampler : register(s1);

float SampleCascadeShadowMap(int index, float2 uv)
{
    if (index == 0) return CascadeShadowMap0.Sample(ShadowSampler, uv).r;
    if (index == 1) return CascadeShadowMap1.Sample(ShadowSampler, uv).r;
    if (index == 2) return CascadeShadowMap2.Sample(ShadowSampler, uv).r;
    return CascadeShadowMap3.Sample(ShadowSampler, uv).r;
}

float CalcCascadeShadow(float3 worldPosition)
{
    float shadow = 1.0f;

    [unroll]
    for (int index = 0; index < CascadeShadowCount; ++index)
    {
        float4 lightPosition = mul(float4(worldPosition, 1.0f), cascadeLightViewProjections[index]);
        lightPosition /= lightPosition.w;
        lightPosition.y = -lightPosition.y;
        lightPosition.xy = lightPosition.xy * 0.5f + 0.5f;

        if (lightPosition.z >= 0.0f && lightPosition.z <= 1.0f &&
            lightPosition.x >= 0.0f && lightPosition.x <= 1.0f &&
            lightPosition.y >= 0.0f && lightPosition.y <= 1.0f)
        {
            float shadowDepth = SampleCascadeShadowMap(index, lightPosition.xy);
            shadow = (lightPosition.z > shadowDepth + cascadeShadowBias[index]) ? cascadeShadowAttenuation : 1.0f;
            break;
        }
    }

    return shadow;
}