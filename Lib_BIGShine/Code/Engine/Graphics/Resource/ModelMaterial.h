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
    // 既存の結合マップと、個別に指定する物性マップの用途。
    enum class TextureSlot
    {
        BASE_COLOR,
        NORMAL,
        EMISSIVE,
        OCCLUSION,
        METALNESS_ROUGHNESS,
        METALLIC,
        ROUGHNESS,
        COUNT
    };
    static constexpr std::size_t TEXTURE_SLOT_COUNT = static_cast<std::size_t>(TextureSlot::COUNT);

    // 物性値として読み取る画像内の成分。GPUへ渡す成分番号と一致させる。
    enum class TextureChannel
    {
        R, G, B, A
    };
    // 結合マップの二つの物性と単独マップを独立して設定する。
    enum class MaterialValue
    {
        PACKED_METALLIC,
        PACKED_ROUGHNESS,
        METALLIC,
        ROUGHNESS,
        COUNT
    };
    static constexpr std::size_t MATERIAL_VALUE_COUNT = static_cast<std::size_t>(MaterialValue::COUNT);

    // 画像を再ロードせず、物性値の参照チャンネルを変更する。
    void SetChannel(MaterialValue value_, TextureChannel channel_);
    TextureChannel GetChannel(MaterialValue value_) const;
    // 設定のない旧JSONや元モデルには、従来のB/G/R/Rを適用する。
    static TextureChannel GetDefaultChannel(MaterialValue value_);
    // 各物性のチャンネル設定と対応するテクスチャスロットを取得する。
    static TextureSlot GetValueTextureSlot(MaterialValue value_);

    // 空のパスを設定すると、このスロットの差し替えを解除する。
    void SetTexture(TextureSlot slot_, const std::string& path_);
    const std::string& GetTexturePath(TextureSlot slot_) const;
    ID3D11ShaderResourceView* GetTexture(TextureSlot slot_) const;
    bool HasTextureFailure(TextureSlot slot_) const;

    // デバイス準備後に未ロードのスロットだけ読み込み、失敗時は元の表示を維持する。
    void LoadPendingTextures(ID3D11Device* device_);
    // 読み込みに失敗したファイルも含め、次のロードで再読み込みする。
    void ReloadTextures();
    // パスと参照チャンネルを保存し、GPUリソースは復元後に再生成する。
    void Serialize(nlohmann::json& json_) const;
    void Deserialize(const nlohmann::json& json_);

private:
    std::array<TextureChannel, MATERIAL_VALUE_COUNT> channels{
        TextureChannel::B, TextureChannel::G, TextureChannel::R, TextureChannel::R
    };
    std::array<std::string, TEXTURE_SLOT_COUNT> texturePaths{};
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, TEXTURE_SLOT_COUNT> textures{};
    std::array<bool, TEXTURE_SLOT_COUNT> pendingLoads{};
    std::array<bool, TEXTURE_SLOT_COUNT> textureFailures{};
};
