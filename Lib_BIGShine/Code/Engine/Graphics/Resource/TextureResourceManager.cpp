#include "PrecompiledHeader.h"

#include "TextureResourceManager.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <filesystem>

TextureResourceManager& TextureResourceManager::Instance()
{
    // TextureはMaterialやSpriteなど複数箇所から共有されるため、Manager側で一元化する。
    static TextureResourceManager instance_;
    return instance_;
}

ID3D11ShaderResourceView* TextureResourceManager::LoadTexture(ID3D11Device* device_, const std::string& path_)
{
    // Device未生成時や空PathではGPU Resourceを作れないため失敗扱いにする。
    if (!device_ || path_.empty())
    {
        return nullptr;
    }
    // 既に読み込んだTextureはSRVを再利用し、同じ画像の二重ロードを避ける。
    const auto iterator_ = textures.find(path_);
    if (iterator_ != textures.end())
    {
        return iterator_->second.Get();
    }

    const std::filesystem::path texturePath_ = WindowsDialogHelper::Utf8ToPath(path_);
    // UTF-8文字列PathをWindows Pathへ変換したうえで、実在しない場合は読み込みを中止する。
    if (!std::filesystem::exists(texturePath_))
    {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_;
    if (FAILED(CPUResourceLoader::LoadTextureFromFile(
        device_, texturePath_.wstring().c_str(), texture_.GetAddressOf())))
    {
        return nullptr;
    }
    // MapにはComPtrを保持し、呼び出し側には描画用の生ポインタだけ返す。
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
    // Device破棄やScene切り替え時にSRV参照をまとめて解放する。
    textures.clear();
}
