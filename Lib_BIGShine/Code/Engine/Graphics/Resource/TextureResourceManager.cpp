#include "PrecompiledHeader.h"

#include "TextureResourceManager.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <filesystem>

TextureResourceManager& TextureResourceManager::Instance()
{
    static TextureResourceManager instance_;
    return instance_;
}

ID3D11ShaderResourceView* TextureResourceManager::LoadTexture(ID3D11Device* device_, const std::string& path_)
{
    if (!device_ || path_.empty()) return nullptr;
    const auto iterator_ = textures.find(path_);
    if (iterator_ != textures.end()) return iterator_->second.Get();

    const std::filesystem::path texturePath_ = WindowsDialogHelper::Utf8ToPath(path_);
    if (!std::filesystem::exists(texturePath_)) return nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
    if (FAILED(CPUResourceLoader::LoadTextureFromFile(
        device_, texturePath_.wstring().c_str(), texture_.GetAddressOf())))
    {
        return nullptr;
    }
    ID3D11ShaderResourceView* result_ = texture_.Get();
    textures.emplace(path_, std::move(texture_));
    return result_;
}

ID3D11ShaderResourceView* TextureResourceManager::GetTexture(const std::string& path_) const
{
    const auto iterator_ = textures.find(path_);
    return iterator_ == textures.end() ? nullptr : iterator_->second.Get();
}

void TextureResourceManager::Clear()
{
    textures.clear();
}
