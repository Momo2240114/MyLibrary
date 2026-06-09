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
    DirectX::XMFLOAT2 emissionSpeed = { 0, 0 };
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
    float noiseScale = 0.0f;      // 互換用: 旧Noise Scale。0以外ならnoiseStrengthとして扱う
    float gravity = -9.8f;       // Y方向の加速度。物理寄りの初期値: -9.8m/s^2

    // Niagara / Effekseer寄りのGPU側モジュール設定
    float noiseFrequency = 1.0f;  // ノイズの細かさ。大きいほど細かく揺れる
    float noiseSpeed = 1.0f;      // ノイズ場のスクロール速度
    float noiseStrength = 0.0f;   // ノイズ加速度の強さ。旧noiseScaleよりこっちを使う
    float drag = 0.0f;            // 空気抵抗。0=なし、値が大きいほど減速

    DirectX::XMFLOAT4 windVelocity = { 0, 0, 0, 0 }; // xyz: 追加加速度。旧名Wind。Acceleration XYZとしてGUI表示

    float radialVelocity = 0.0f;  // エミッター中心から外へ押し出す速度加算
    float orbitVelocity = 0.0f;   // Y軸周りの回転速度加算
    float stretchScale = 0.04f;   // Stretched Billboardの伸び率
    float stretchMax = 100.0f;    // Stretched Billboardの最大長

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

    DirectX::XMUINT2 textureSplitCount = { 1, 1 };
    DirectX::XMUINT2 spriteSheetIndex = { 0, 0 };

    uint32_t useSpriteAnimation = 0;
    uint32_t spriteAnimationLoop = 1;
    uint32_t spriteAnimationStartIndex = 0;
    uint32_t spriteAnimationEndIndex = 0;

    float spriteAnimationFps = 12.0f;
    DirectX::XMFLOAT3 spritePadding = { 0, 0, 0 };

    uint32_t useNoiseTexture = 0;
    DirectX::XMFLOAT3 padding = { 0, 0, 0 };
};

struct particle_system_settings
{
    ParticleSystemConstants data;
    std::string texture_path;
    std::string color_temper_path;
};
