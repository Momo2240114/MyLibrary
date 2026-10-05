#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>

// メッシュや元のマテリアルを変更せず、差し替えるテクスチャを保持する。
class ModelMaterial
{
public:
    // モデルシェーダーのt0～t4に対応するテクスチャ用途。
    enum class TextureSlot
    {
        BASE_COLOR,
        NORMAL,
        EMISSIVE,
        OCCLUSION,
        METALNESS_ROUGHNESS,
        COUNT
    };
    static constexpr std::size_t TEXTURE_SLOT_COUNT = static_cast<std::size_t>(TextureSlot::COUNT);

    // 空のパスを設定すると、このスロットの差し替えを解除する。
    void SetTexture(TextureSlot slot_, const std::string& path_);
    const std::string& GetTexturePath(TextureSlot slot_) const;
    ID3D11ShaderResourceView* GetTexture(TextureSlot slot_) const;
    bool HasTextureFailure(TextureSlot slot_) const;

    // デバイス準備後に未ロードのスロットだけ読み込み、失敗時は元の表示を維持する。
    void LoadPendingTextures(ID3D11Device* device_);
    // 読み込みに失敗したファイルも含め、次のロードで再読み込みする。
    void ReloadTextures();
    // 保存にはパスだけを使用し、GPUリソースは復元後に再生成する。
    void Serialize(nlohmann::json& json_) const;
    void Deserialize(const nlohmann::json& json_);

private:
    std::array<std::string, TEXTURE_SLOT_COUNT> texturePaths{};
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, TEXTURE_SLOT_COUNT> textures{};
    std::array<bool, TEXTURE_SLOT_COUNT> pendingLoads{};
    std::array<bool, TEXTURE_SLOT_COUNT> textureFailures{};
};