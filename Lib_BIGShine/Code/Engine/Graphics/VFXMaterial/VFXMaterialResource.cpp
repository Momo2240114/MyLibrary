#include "PrecompiledHeader.h"

#include "VFXMaterialResource.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Debug/TracyProfiler.h"

#include <cstdio>

bool VFXMaterialResource::ApplyLoadedData(const VFXMaterialData& materialData_)
{
    BS_PROFILE_SCOPE("VFXMaterialResource::ApplyLoadedData");
    materialData = materialData_;
    return true;
}

bool VFXMaterialResource::LoadTextures(ID3D11Device* device_)
{
    BS_PROFILE_SCOPE("VFXMaterialResource::LoadTextures");
    if (!device_) return false;
    baseTexture = LoadTextureSlot(device_, materialData.baseTexturePath, 0xFFFFFFFFu);
    maskTexture = LoadTextureSlot(device_, materialData.maskTexturePath, 0xFFFFFFFFu);
    noiseTexture = LoadTextureSlot(device_, materialData.noiseTexturePath, 0xFFFFFFFFu);
    normalTexture = LoadTextureSlot(device_, materialData.normalTexturePath, 0x8080FFFFu);
    distortionTexture = LoadTextureSlot(device_, materialData.distortionTexturePath, 0x80808080u);
    return baseTexture != nullptr;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> VFXMaterialResource::LoadTextureSlot(
    ID3D11Device* device_,
    const std::string& path_,
    uint32_t fallbackColor_) const
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
    if (!path_.empty())
    {
        ID3D11ShaderResourceView* loadedTexture_ =
            TextureResourceManager::Instance().LoadTexture(device_, path_);
        if (loadedTexture_)
        {
            texture_ = loadedTexture_;
            return texture_;
        }
        std::printf("[VFXMaterialResource] Texture fallback: %s\n", path_.c_str());
    }
    CPUResourceLoader::CreateDummyTexture(
        device_,
        fallbackColor_,
        texture_.ReleaseAndGetAddressOf());
    return texture_;
}
