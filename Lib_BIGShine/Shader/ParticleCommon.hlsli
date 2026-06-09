#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

#include "Scene.hlsli"

static const uint NumParticleThread = 1024;

static const uint IndirectArgumentsNumCurrentParticle = 0;
static const uint IndirectArgumentsNumPreviousParticle = 4;
static const uint IndirectArgumentsNumDeadParticle = 8;
static const uint IndirectArgumentsEmitParticleDispatchIndirect = 12;
static const uint IndirectArgumentsUpdateParticleDispatchIndirect = 24;
static const uint IndirectArgumentsNumEmitParticleIndex = 36;
static const uint IndirectArgumentsDrawIndirect = 40;

struct Particle
{
    float4 parameter;
    float4 position;
    float4 rotation;
    float4 scale;
    float4 velocity;
    float4 acceleration;
    float4 texcoord;
    float4 color;
};

struct EmitParticleData
{
    float4 parameter;
    float4 position;
    float4 rotation;
    float4 scale;
    float4 velocity;
    float4 acceleration;
    float4 color;
};

struct ParticleHeader
{
    uint alive;
    uint particleIndex;
    float depth;
    uint dummy;
};

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
    float4 colorStart;
    float4 colorEnd;
    float2 sizeRandomStart;
    float2 sizeRandomEnd;
    float time;
    float deltaTime;
    float noiseScale;
    float gravity; // Y acceleration. Default is -9.8 on C++ side.

    float noiseFrequency;
    float noiseSpeed;
    float noiseStrength;
    float drag;

    float4 windVelocity; // xyz: additional acceleration. Kept name for compatibility.

    float radialVelocity;
    float orbitVelocity;
    float stretchScale;
    float stretchMax;

    uint maxParticleCount;
    uint particleRenderType;
    uint emissionMode;
    uint burstCount;
    float burstTrigger;
    uint spawnShape;
    uint useMaskTexture;
    uint useGradientTexture;
    float4 shapeStart;
    float4 shapeEnd;
    float4 shapeSize;
    uint useColorOverLife;
    uint useRandomSize;
    uint padding0;
    uint padding1;
    uint2 textureSplitCountSetting;
    uint2 spriteSheetIndex;
    uint useSpriteAnimation;
    uint spriteAnimationLoop;
    uint spriteAnimationStartIndex;
    uint spriteAnimationEndIndex;
    float spriteAnimationFps;
    float3 spritePadding;
    uint useNoiseTexture;
    float3 padding;
};

cbuffer ParticleCommonConstants : register(b10)
{
    float elapsedTime;
    uint2 textureSplitCount;
    uint systemNumParticles;
    uint totalEmitCount;
    uint commonDummy[3];
};

cbuffer ParticleBitonicSortConstants : register(b11)
{
    uint increment;
    uint direction;
    uint sortDummy[2];
};

static const uint BitonicSortB2Thread = 256;
static const uint BitonicSortC2Thread = 512;

struct VS_OUT
{
    uint vertexID : TEXCOORD0;
};

struct GS_OUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 tex : TEXCOORD0;
    float lifeRate : TEXCOORD1;
};

#endif
