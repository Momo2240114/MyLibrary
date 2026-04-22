#pragma once

#include <directxmath.h>
#include <string>
#include <vector.h>



struct Particle {
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
    float             age = 0.0f;

    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
    float             lifeSpan = 0.0f;
    DirectX::XMFLOAT3 acceleration = { 0, 0, 0 };
    float             rotation = 0.0f;

    DirectX::XMFLOAT4 startColor = { 1, 1, 1, 1 };
    DirectX::XMFLOAT4 endColor = { 1, 1, 1, 1 };

    DirectX::XMFLOAT2 size = { 1, 1 };
    uint32_t          type = 0;
    uint32_t          isActive = 0; // 0 (false) で初期化
};


// Compute Shader用の定数バッファ (b9)
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

    DirectX::XMFLOAT4   manualColor = { 1, 1, 1, 1 };

    float   time = 0.0f;
    float   deltaTime = 0.0f;
    float   noiseScale = 0.0f;
    float   gravity = 0.0f;

    uint32_t maxParticleCount = 1000;
    uint32_t type = 0;
    uint32_t emissionMode = 0;
    uint32_t burstCount = 0;

    float    burstTrigger = 0.0f;
    DirectX::XMFLOAT3 padding = { 0, 0, 0 };
};

// 3. 全体の描画設定をまとめた構造体
struct particle_system_settings
{
    ParticleSystemConstants data;
    std::string texture_path;
    std::string color_temper_path;
};