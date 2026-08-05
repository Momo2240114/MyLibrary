#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class TextureResourceManager
{
public:
    // Textureを共有管理するSingleton。
    static TextureResourceManager& Instance();

    // 指定PathのTextureを読み込み、読み込み済みならCache済みSRVを返す。
    ID3D11ShaderResourceView* LoadTexture(ID3D11Device* device_, const std::string& path_);
    // Cache内のTextureだけを取得する。未読み込みならnullptr。
    ID3D11ShaderResourceView* GetTexture(const std::string& path_) const;
    // 読み込み済みTextureを全て解放する。
    void Clear();

private:
    // PathをKeyにしたTexture SRV Cache。
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
};
