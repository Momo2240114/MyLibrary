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
    static VFXMaterialData FromMeshVFXPart(
        const MeshVFXPart& part_,
        const std::string& materialName_);
    static VFXMaterialData FromParticleEmitter(
        const ParticleSystemConstants& settings_,
        const std::string& emitterName_,
        const std::string& texturePath_,
        const std::string& maskTexturePath_,
        ParticleBlendMode blendMode_);
    static VFXMaterialData FromDecal(
        const std::string& materialName_,
        const std::string& texturePath_,
        const DirectX::XMFLOAT4& color_);
};
