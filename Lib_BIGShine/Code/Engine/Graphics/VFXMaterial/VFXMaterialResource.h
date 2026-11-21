#pragma once

#include "VFXMaterialData.h"

#include <cstdint>
#include <d3d11.h>
#include <string>
#include <wrl.h>

class VFXMaterialResource
{
public:
    // 描画に使うMaterial設定を読み取り専用で取得する。
    const VFXMaterialData& GetMaterialData() const
    {
        return materialData;
    }

    // Editor編集用にMaterialDataへ直接アクセスする。
    VFXMaterialData& GetEditableMaterialData()
    {
        return materialData;
    }

    // MaterialData内のTexture PathからSRVを読み込み、足りないSlotはDummy Textureで補う。
    bool LoadTextures(ID3D11Device* device_);
    // Serializerなどで読み込んだDataをResourceへ反映する。
    bool ApplyLoadedData(const VFXMaterialData& materialData_);

    const std::string& GetSourceFilePath() const
    {
        return sourceFilePath;
    }

    // Assetとして保存・再読み込みする時の元ファイルパスを保持する。
    void SetSourceFilePath(const std::string& sourceFilePath_)
    {
        sourceFilePath = sourceFilePath_;
    }

    // MeshVFX/Particle/Decalの描画時にShaderへ渡すTexture SRV。
    ID3D11ShaderResourceView* GetBaseTexture() const
    {
        return baseTexture.Get();
    }

    ID3D11ShaderResourceView* GetMaskTexture() const
    {
        return maskTexture.Get();
    }

    ID3D11ShaderResourceView* GetNoiseTexture() const
    {
        return noiseTexture.Get();
    }

    ID3D11ShaderResourceView* GetNormalTexture() const
    {
        return normalTexture.Get();
    }

    ID3D11ShaderResourceView* GetDistortionTexture() const
    {
        return distortionTexture.Get();
    }

private:
    // 1つのTexture Slotを読み込む。Pathが空/失敗時はfallbackColorの1px Textureを返す。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureSlot(
        ID3D11Device* device_,
        const std::string& path_,
        uint32_t fallbackColor_) const;

private:
    // 保存元/読み込み元のMaterial Path。TemporaryやDefaultでは特殊名を入れる。
    std::string sourceFilePath;
    // Shaderへ渡す各種色・数値・Texture Path。
    VFXMaterialData materialData;
    // 各Texture SlotのShaderResourceView。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distortionTexture;
};
