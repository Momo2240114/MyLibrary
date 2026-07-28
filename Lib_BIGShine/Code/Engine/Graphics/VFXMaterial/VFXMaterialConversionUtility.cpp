#include "PrecompiledHeader.h"

#include "VFXMaterialConversionUtility.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Particle/ParticleRessiource.h"
#include "Engine/Graphics/VFX/MeshVFXResource.h"

namespace
{
    VFX_MATERIAL_BLEND_MODE ConvertBlendMode(MeshVFXBlendMode blendMode_)
    {
        return blendMode_ == MeshVFXBlendMode::ADDITIVE
            ? VFX_MATERIAL_BLEND_MODE::ADDITIVE
            : VFX_MATERIAL_BLEND_MODE::ALPHA;
    }

    VFX_MATERIAL_BLEND_MODE ConvertBlendMode(ParticleBlendMode blendMode_)
    {
        return blendMode_ == ParticleBlendMode::ADDITIVE
            ? VFX_MATERIAL_BLEND_MODE::ADDITIVE
            : VFX_MATERIAL_BLEND_MODE::ALPHA;
    }
}

VFXMaterialData VFXMaterialConversionUtility::FromMeshVFXPart(
    const MeshVFXPart& part_,
    const std::string& materialName_)
{
    BS_PROFILE_SCOPE("VFXMaterialConversionUtility::FromMeshVFXPart");

    VFXMaterialData materialData_{};
    materialData_.materialName = materialName_.empty() ? "ConvertedMeshVFXMaterial" : materialName_;
    materialData_.baseTexturePath = part_.baseTexturePath;
    materialData_.maskTexturePath = part_.maskTexturePath;
    materialData_.noiseTexturePath = part_.noiseTexturePath;
    materialData_.normalTexturePath = part_.normalTexturePath;
    materialData_.distortionTexturePath = part_.distortionNoiseTexturePath;
    materialData_.baseColor = part_.color;
    materialData_.alpha = part_.alpha;
    materialData_.emissiveColor = { part_.color.x, part_.color.y, part_.color.z };
    materialData_.emissiveIntensity = part_.emissiveStrength;
    materialData_.mainUV.scale = part_.baseUVScale;
    materialData_.mainUV.scrollSpeed = part_.baseUVSpeed;
    materialData_.mainUV.rotation = part_.baseUVRotation;
    materialData_.mainUV.rotationSpeed = part_.baseUVRotationSpeed;
    materialData_.noiseUV.scale = part_.noiseUVScale;
    materialData_.noiseUV.scrollSpeed = part_.noiseUVSpeed;
    materialData_.noiseUV.rotation = part_.noiseUVRotation;
    materialData_.noiseUV.rotationSpeed = part_.noiseUVRotationSpeed;
    materialData_.dissolve.enabled = part_.useDissolveEdge || part_.dissolveThreshold > 0.0f;
    materialData_.dissolve.threshold = part_.dissolveThreshold;
    materialData_.dissolve.edgeWidth = part_.dissolveEdgeWidth;
    materialData_.dissolve.edgeColor = part_.dissolveEdgeColor;
    materialData_.distortion.enabled = part_.useMaskDistortion;
    materialData_.distortion.strength = part_.maskDistortionStrength;
    materialData_.renderState.blendMode = ConvertBlendMode(part_.blendMode);
    materialData_.renderState.cullMode = VFX_MATERIAL_CULL_MODE::NONE;
    materialData_.renderState.depthTest = true;
    materialData_.renderState.depthWrite = false;
    return materialData_;
}

VFXMaterialData VFXMaterialConversionUtility::FromParticleEmitter(
    const ParticleSystemConstants& settings_,
    const std::string& emitterName_,
    const std::string& texturePath_,
    const std::string& maskTexturePath_,
    ParticleBlendMode blendMode_)
{
    BS_PROFILE_SCOPE("VFXMaterialConversionUtility::FromParticleEmitter");

    VFXMaterialData materialData_{};
    materialData_.materialName = emitterName_.empty() ? "ConvertedParticleMaterial" : emitterName_ + "_Material";
    materialData_.baseTexturePath = texturePath_;
    materialData_.maskTexturePath = maskTexturePath_;
    materialData_.baseColor = settings_.manualColor;
    materialData_.alpha = settings_.manualColor.w;
    materialData_.colorRamp.enabled = settings_.useColorOverLife != 0u;
    materialData_.colorRamp.sampleSource = VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::LIFE;
    materialData_.colorRamp.blendMode = VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::MULTIPLY;
    materialData_.colorRamp.stops =
    {
        { 0.0f, settings_.colorStart },
        { 1.0f, settings_.colorEnd },
    };
    materialData_.renderState.blendMode = ConvertBlendMode(blendMode_);
    materialData_.renderState.cullMode = VFX_MATERIAL_CULL_MODE::NONE;
    materialData_.renderState.depthTest = true;
    materialData_.renderState.depthWrite = false;
    return materialData_;
}

VFXMaterialData VFXMaterialConversionUtility::FromDecal(
    const std::string& materialName_,
    const std::string& texturePath_,
    const DirectX::XMFLOAT4& color_)
{
    BS_PROFILE_SCOPE("VFXMaterialConversionUtility::FromDecal");

    VFXMaterialData materialData_{};
    materialData_.materialName = materialName_.empty() ? "ConvertedDecalMaterial" : materialName_;
    materialData_.baseTexturePath = texturePath_;
    materialData_.baseColor = color_;
    materialData_.alpha = color_.w;
    materialData_.renderState.blendMode = VFX_MATERIAL_BLEND_MODE::ALPHA;
    materialData_.renderState.cullMode = VFX_MATERIAL_CULL_MODE::NONE;
    materialData_.renderState.depthTest = true;
    materialData_.renderState.depthWrite = false;
    return materialData_;
}
