#pragma once

#include <directxmath.h>
#include <cstdint>
#include <string>
#include <vector.h>

struct Particle
{
    DirectX::XMFLOAT4 parameter = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 position = { 0, 0, 0, 1 };
    DirectX::XMFLOAT4 rotation = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 scale = { 1, 1, 1, 0 };
    DirectX::XMFLOAT4 velocity = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 acceleration = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 texcoord = { 0, 0, 1, 1 };
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };
};

struct EmitParticleData
{
    DirectX::XMFLOAT4 parameter = { 0, 1, 0, 0 };
    DirectX::XMFLOAT4 position = { 0, 0, 0, 1 };
    DirectX::XMFLOAT4 rotation = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 scale = { 1, 1, 1, 0 };
    DirectX::XMFLOAT4 velocity = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 acceleration = { 0, 0, 0, 0 };
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };
};

struct ParticleHeader
{
    uint32_t alive = 0;
    uint32_t particleIndex = 0;
    float depth = 0.0f;
    uint32_t dummy = 0;
};

struct ParticleSystemConstants
{
    DirectX::XMFLOAT4 emissionPosition = { 0, 0, 0, 1 };
    DirectX::XMFLOAT2 emissionOffset = { 0, 0 };
    DirectX::XMFLOAT2 emissionSize = { 1, 1 };
    DirectX::XMFLOAT2 emissionConeAngle = { 0, 0 };
    DirectX::XMFLOAT2 emissionSpeed = { 1, 1 };
    DirectX::XMFLOAT2 emissionAngularSpeed = { 0, 0 };
    DirectX::XMFLOAT2 lifespan = { 1, 1 };
    DirectX::XMFLOAT2 spawnDelay = { 0, 0 };
    DirectX::XMFLOAT2 fadeDuration = { 0, 0 };
    DirectX::XMFLOAT4 manualColor = { 1, 1, 1, 1 };
    DirectX::XMFLOAT4 colorStart = { 1, 1, 1, 1 };
    DirectX::XMFLOAT4 colorEnd = { 1, 1, 1, 1 };
    DirectX::XMFLOAT2 sizeRandomStart = { 1, 1 };
    DirectX::XMFLOAT2 sizeRandomEnd = { 1, 1 };

    float time = 0.0f;
    float deltaTime = 0.0f;
    float noiseScale = 0.0f;
    float gravity = 0.0f;

    uint32_t maxParticleCount = 1000;
    uint32_t type = 0;
    uint32_t emissionMode = 0;
    uint32_t burstCount = 0;

    float burstTrigger = 0.0f;
    uint32_t spawnShape = 0;
    uint32_t useMaskTexture = 0;
    uint32_t useGradientTexture = 0;

    DirectX::XMFLOAT4 shapeStart = { 0, 0, 0, 1 };
    DirectX::XMFLOAT4 shapeEnd = { 0, 0, 0, 1 };
    DirectX::XMFLOAT4 shapeSize = { 1, 1, 1, 0 };

    uint32_t useColorOverLife = 0;
    uint32_t useRandomSize = 0;
    uint32_t padding0 = 0;
    uint32_t padding1 = 0;

    uint32_t useNoiseTexture = 0;
    DirectX::XMFLOAT3 padding = { 0, 0, 0 };
};

struct particle_system_settings
{
    ParticleSystemConstants data;
    std::string texture_path;
    std::string color_temper_path;
};
