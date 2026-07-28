#pragma once

#include "VFXMaterialData.h"

#include <cstdint>
#include <d3d11.h>
#include <string>
#include <wrl.h>

class VFXMaterialResource
{
public:
    const VFXMaterialData& GetMaterialData() const { return materialData; }
    VFXMaterialData& GetEditableMaterialData() { return materialData; }

    bool LoadTextures(ID3D11Device* device_);
    bool ApplyLoadedData(const VFXMaterialData& materialData_);

    const std::string& GetSourceFilePath() const { return sourceFilePath; }
    void SetSourceFilePath(const std::string& sourceFilePath_) { sourceFilePath = sourceFilePath_; }

    ID3D11ShaderResourceView* GetBaseTexture() const { return baseTexture.Get(); }
    ID3D11ShaderResourceView* GetMaskTexture() const { return maskTexture.Get(); }
    ID3D11ShaderResourceView* GetNoiseTexture() const { return noiseTexture.Get(); }
    ID3D11ShaderResourceView* GetNormalTexture() const { return normalTexture.Get(); }
    ID3D11ShaderResourceView* GetDistortionTexture() const { return distortionTexture.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureSlot(
        ID3D11Device* device_,
        const std::string& path_,
        uint32_t fallbackColor_) const;

private:
    std::string sourceFilePath;
    VFXMaterialData materialData;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distortionTexture;
};
