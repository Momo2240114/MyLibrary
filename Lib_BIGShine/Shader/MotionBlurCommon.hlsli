#ifndef MOTION_BLUR_COMMON_HLSLI
#define MOTION_BLUR_COMMON_HLSLI

#define MOTION_BLUR_TILE_SIZE 20

cbuffer CbMotionBlur : register(b0)
{
    int2 screenSize;
    float2 screenPixelSize;
    float4x4 invViewProjection;
    float4x4 previousViewProjection;
    int enabled;
    int sampleCount;
    float strength;
    float maxVelocity;
}

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

#endif
