#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <wrl.h>

class TextureResourceManager
{
public:
    static TextureResourceManager& Instance();

    ID3D11ShaderResourceView* LoadTexture(ID3D11Device* device_, const std::string& path_);
    ID3D11ShaderResourceView* GetTexture(const std::string& path_) const;
    void Clear();

private:
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textures;
};
