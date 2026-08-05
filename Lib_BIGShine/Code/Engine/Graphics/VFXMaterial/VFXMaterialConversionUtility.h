#pragma once

#include "VFXMaterialData.h"

#include <DirectXMath.h>
#include <string>

struct MeshVFXPart;
struct ParticleSystemConstants;
enum class MeshVFXBlendMode;
enum class ParticleBlendMode;

class VFXMaterialConversionUtility
{
public:
    // MeshVFXPartに直接持っている色・UV・Blend設定からVFXMaterialDataを作る。
    static VFXMaterialData FromMeshVFXPart(
            const MeshVFXPart& part_,
            const std::string& materialName_);

    // ParticleEmitterの設定とTexture Pathを、共通VFXMaterial形式へ変換する。
    static VFXMaterialData FromParticleEmitter(
        const ParticleSystemConstants& settings_,
        const std::string& emitterName_,
        const std::string& texturePath_,
            const std::string& maskTexturePath_,
            ParticleBlendMode blendMode_);

    // 旧DecalのTexture/Color指定を、VFXMaterial形式へ変換する。
    static VFXMaterialData FromDecal(
        const std::string& materialName_,
        const std::string& texturePath_,
        const DirectX::XMFLOAT4& color_);
};
