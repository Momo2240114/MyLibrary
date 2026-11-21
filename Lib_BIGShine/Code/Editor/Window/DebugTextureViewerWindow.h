#pragma once

#include "EditorWindow.h"

#include <d3d11.h>
#include <array>
#include <vector>
#include <wrl/client.h>

class DebugTextureViewerWindow : public EditorWindow
{
public:
    explicit DebugTextureViewerWindow(GameObjectManager* manager_)
        : EditorWindow("Debug Texture Viewer", manager_)
    {
    }

    void Draw() override;

private:
    struct TextureEntry
    {
        // 表示名とSRVの組。SRVがnullptrなら欠損テクスチャとして扱う。
        const char* name;
        ID3D11ShaderResourceView* srv;
        int channel = -1; // -1は通常表示、0～3はRGBA各成分の白黒表示。
    };

    void DrawTextureGrid(const std::vector<TextureEntry>& textures_);
    void DrawTexture(const TextureEntry& texture_);
    bool EnsureChannelShaders();
    void AddSceneTextures(std::vector<TextureEntry>& textures_);
    void AddGBufferTextures(std::vector<TextureEntry>& textures_);
    void AddPostEffectTextures(std::vector<TextureEntry>& textures_);
    void AddShadowTextures(std::vector<TextureEntry>& textures_);
    ImVec2 GetTextureImageSize(ID3D11ShaderResourceView* srv_) const;

    float thumbnailSize = 220.0f;
    // サムネイルの縦横比維持と、未生成SRVを表示するかどうかのUI状態。
    bool keepAspect = true;
    bool showMissingTextures = true;
    std::array<Microsoft::WRL::ComPtr<ID3D11PixelShader>, 4> channelShaders;
    bool channelShaderInitializationAttempted = false;
};
