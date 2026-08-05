#pragma once

#include "ParticleEmitter.h"

#include <cstdint>
#include <string>
#include <vector>

struct ParticleEmitterAsset
{
    // Emitter本体のGPU更新・描画パラメータ。
    ParticleSystemConstants settings;

    // ActionやEditorで識別するEmitter名。
    std::string name = "Emitter";

    // VFXMaterialを使う場合のMaterialパス。複数指定時は時間で切り替える。
    std::string materialPath;
    std::vector<std::string> materialPaths;
    std::string texturePath;
    std::string textureFileName;
    std::string maskTexturePath;
    std::string maskTextureFileName;
    std::string gradientTexturePath;
    std::string gradientTextureFileName;
    std::string shadeGradientTexturePath;
    std::string shadeGradientTextureFileName;

    // Effect再生開始からこのEmitterを発火するまでの待ち時間。
    float delay = 0.0f;

    // MeshVertices発生時にサンプリングする最大頂点数。
    uint32_t meshVertexSampleCount = 2048;

    // 描画合成方法と、発生位置の座標空間。
    ParticleBlendMode blendMode = ParticleBlendMode::ALPHA;
    ParticleSpawnSpace spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
};

struct ParticleEffectAsset
{
    // 1つのParticle Effectに含まれる複数Emitter。
    std::vector<ParticleEmitterAsset> emitters;

    // Asset保存時のデフォルト再生状態。
    bool isPlaying = false;
};
