#include "PrecompiledHeader.h"

#include "VFXMaterialResource.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Debug/TracyProfiler.h"

#include <cstdio>

bool VFXMaterialResource::ApplyLoadedData(const VFXMaterialData& materialData_)
{
    BS_PROFILE_SCOPE("VFXMaterialResource::ApplyLoadedData");
    // JSONなどから復元した設定を丸ごと保持する。
    // Texture本体は別途LoadTexturesでDevice依存Resourceとして読み込む。
    materialData = materialData_;
    return true;
}

bool VFXMaterialResource::LoadTextures(ID3D11Device* device_)
{
    BS_PROFILE_SCOPE("VFXMaterialResource::LoadTextures");
    if (!device_)
    {
        return false;
    }
    // Slotごとに意味のあるFallback色を使う。
    // Normalはフラット法線、Mask/Baseは白、Distortionは中間値で無効に近い状態へ倒す。
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
        // TextureResourceManager経由で読み、同じPathのTextureを共有する。
        ID3D11ShaderResourceView* loadedTexture_ =
            TextureResourceManager::Instance().LoadTexture(device_, path_);
        if (loadedTexture_)
        {
            texture_ = loadedTexture_;
            return texture_;
        }
        std::printf("[VFXMaterialResource] Texture fallback: %s\n", path_.c_str());
    }
    // Path未設定または読み込み失敗時でもShader側がnullを触らないようDummyを作る。
    CPUResourceLoader::CreateDummyTexture(
        device_,
        fallbackColor_,
        texture_.ReleaseAndGetAddressOf());
    return texture_;
}
