#pragma once

#include "ParticleEmitter.h"

#include <cstdint>
#include <string>
#include <vector>

struct ParticleEmitterAsset
{
    ParticleSystemConstants settings;

    std::string name = "Emitter";
    std::string texturePath;
    std::string textureFileName;
    std::string maskTexturePath;
    std::string maskTextureFileName;
    std::string gradientTexturePath;
    std::string gradientTextureFileName;

    float delay = 0.0f;
    uint32_t meshVertexSampleCount = 2048;
    ParticleBlendMode blendMode = ParticleBlendMode::ALPHA;
    ParticleSpawnSpace spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
};

struct ParticleEffectAsset
{
    std::vector<ParticleEmitterAsset> emitters;
    bool isPlaying = false;
};
