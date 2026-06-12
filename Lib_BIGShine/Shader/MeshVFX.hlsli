#ifndef MESH_VFX_HLSLI
#define MESH_VFX_HLSLI

#include "Scene.hlsli"

cbuffer CbMeshVFX : register(b0)
{
    row_major float4x4 world;
    float4 vfxColor;
    float2 baseUVSpeed;
    float2 maskUVSpeed;
    float2 noiseUVSpeed;
    float2 uvRotationCenter;
    float time;
    float alpha;
    float normalStrength;
    float noiseStrength;
    float fresnelPower;
    float fresnelStrength;
    float dissolveThreshold;
    float baseUVRotation;
    float maskUVRotation;
    float noiseUVRotation;
    uint useMask;
    uint useNoise;
    uint useNormal;
    uint useFresnel;
    uint2 padding;
};

struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    float4 boneWeights : BONE_WEIGHTS;
    uint4 boneIndices : BONE_INDICES;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float4 tangent : TEXCOORD3;
};

#endif
