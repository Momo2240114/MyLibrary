#pragma once

#include "EditorWindow.h"

#include <d3d11.h>
#include <vector>

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
        const char* name;
        ID3D11ShaderResourceView* srv;
    };

    void DrawTextureGrid(const std::vector<TextureEntry>& textures_);
    void DrawTexture(const TextureEntry& texture_);
    void AddSceneTextures(std::vector<TextureEntry>& textures_);
    void AddGBufferTextures(std::vector<TextureEntry>& textures_);
    void AddPostEffectTextures(std::vector<TextureEntry>& textures_);
    void AddShadowTextures(std::vector<TextureEntry>& textures_);
    ImVec2 GetTextureImageSize(ID3D11ShaderResourceView* srv_) const;

    float thumbnailSize = 220.0f;
    bool keepAspect = true;
    bool showMissingTextures = true;
};
