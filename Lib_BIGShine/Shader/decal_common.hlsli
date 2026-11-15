#ifndef __DECAL_COMMON_HLSLI__
#define __DECAL_COMMON_HLSLI__

#include "Scene.hlsli"

cbuffer GBUFFER_DECAL_CONSTANT_BUFFER : register(b10)
{
    row_major float4x4 decal_world_matrix;
    row_major float4x4 decal_inverse_transform;
    float4 decal_direction;
    float4 decalBaseColor;
    float4 decalEmissive;
    float4 decalUvScaleOffset;
    float4 decalNoiseUvScaleOffset;
    float4 decalDissolveParams;
    float4 decalDissolveEdgeColor;
    uint decalBlendMode;
    uint decalUseDissolve;
    uint decalGroundSurface;
    float decalPadding;
};

Texture2D<float4> scene_color_texture : register(t0);
Texture2D<float> scene_depth_texture : register(t4);
Texture2D<float4> scene_normal_texture : register(t2);
Texture2D<float4> decal_texture : register(t10);
Texture2D<float4> decal_mask_texture : register(t11);
Texture2D<float4> decal_noise_texture : register(t12);
SamplerState sampler_states[2] : register(s0);

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 screenPos : TEXCOORD0;
    float4 color : COLOR;
};
#endif