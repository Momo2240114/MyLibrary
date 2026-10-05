#include "PrecompiledHeader.h"

#include "ModelMaterial.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <exception>
#include <filesystem>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

namespace
{
    constexpr const char* TEXTURE_KEYS[] =
    {
        "BaseColor", "Normal", "Emissive", "Occlusion", "MetalnessRoughness"
    };
}

void ModelMaterial::SetTexture(TextureSlot slot_, const std::string& path_)
{
    const std::size_t index_ = static_cast<std::size_t>(slot_);
    if (index_ >= TEXTURE_SLOT_COUNT) return;
    texturePaths[index_] = path_;
    textures[index_].Reset();
    pendingLoads[index_] = !path_.empty();
    textureFailures[index_] = false;
}

const std::string& ModelMaterial::GetTexturePath(TextureSlot slot_) const
{
    const std::size_t index_ = static_cast<std::size_t>(slot_);
    static const std::string EMPTY_PATH;
    return index_ < TEXTURE_SLOT_COUNT ? texturePaths[index_] : EMPTY_PATH;
}

ID3D11ShaderResourceView* ModelMaterial::GetTexture(TextureSlot slot_) const
{
    const std::size_t index_ = static_cast<std::size_t>(slot_);
    return index_ < TEXTURE_SLOT_COUNT ? textures[index_].Get() : nullptr;
}

bool ModelMaterial::HasTextureFailure(TextureSlot slot_) const
{
    const std::size_t index_ = static_cast<std::size_t>(slot_);
    return index_ < TEXTURE_SLOT_COUNT && textureFailures[index_];
}

void ModelMaterial::LoadPendingTextures(ID3D11Device* device_)
{
    if (!device_) return;
    for (std::size_t index_ = 0; index_ < TEXTURE_SLOT_COUNT; ++index_)
    {
        if (!pendingLoads[index_]) continue;
        BS_PROFILE_SCOPE("ModelMaterial::LoadTexture");
        pendingLoads[index_] = false;

        // 色テクスチャはsRGB、法線や物性値は線形として読み込む。
        const bool isColor_ = index_ == static_cast<std::size_t>(TextureSlot::BASE_COLOR) ||
            index_ == static_cast<std::size_t>(TextureSlot::EMISSIVE);
        HRESULT result_ = E_FAIL;
        try
        {
            // 日本語を含むUTF-8パスを、既存の共通処理でWindows用パスへ変換する。
            std::filesystem::path path_ = WindowsDialogHelper::Utf8ToPath(texturePaths[index_]);
            std::filesystem::path ddsPath_ = path_;
            ddsPath_.replace_extension(".dds");
            std::error_code error_;
            if (std::filesystem::is_regular_file(ddsPath_, error_))
            {
                result_ = DirectX::CreateDDSTextureFromFileEx(
                    device_, ddsPath_.c_str(), 0, D3D11_USAGE_DEFAULT,
                    D3D11_BIND_SHADER_RESOURCE, 0, 0,
                    isColor_ ? DirectX::DDS_LOADER_FORCE_SRGB : DirectX::DDS_LOADER_IGNORE_SRGB,
                    nullptr, textures[index_].ReleaseAndGetAddressOf());
            }
            else
            {
                result_ = DirectX::CreateWICTextureFromFileEx(
                    device_, path_.c_str(), 0, D3D11_USAGE_DEFAULT,
                    D3D11_BIND_SHADER_RESOURCE, 0, 0,
                    isColor_ ? DirectX::WIC_LOADER_FORCE_SRGB : DirectX::WIC_LOADER_IGNORE_SRGB,
                    nullptr, textures[index_].ReleaseAndGetAddressOf());
            }
        }
        catch (const std::exception&)
        {
            result_ = E_FAIL;
        }
        textureFailures[index_] = FAILED(result_) || !textures[index_];
        if (textureFailures[index_]) textures[index_].Reset();
    }
}

void ModelMaterial::ReloadTextures()
{
    for (std::size_t index_ = 0; index_ < TEXTURE_SLOT_COUNT; ++index_)
    {
        textures[index_].Reset();
        pendingLoads[index_] = !texturePaths[index_].empty();
        textureFailures[index_] = false;
    }
}

void ModelMaterial::Serialize(nlohmann::json& json_) const
{
    for (std::size_t index_ = 0; index_ < TEXTURE_SLOT_COUNT; ++index_)
    {
        json_[TEXTURE_KEYS[index_]] = texturePaths[index_];
    }
}

void ModelMaterial::Deserialize(const nlohmann::json& json_)
{
    for (std::size_t index_ = 0; index_ < TEXTURE_SLOT_COUNT; ++index_)
    {
        SetTexture(static_cast<TextureSlot>(index_), json_.value(TEXTURE_KEYS[index_], std::string{}));
    }
}