#pragma once

#include "EditorWindow.h"

#include <d3d11.h>
#include <filesystem>
#include <string>
#include <vector>

class AssetBrowserWindow : public EditorWindow
{
public:
    explicit AssetBrowserWindow(GameObjectManager* manager_);
    void Draw() override;
    void DrawContent();

private:
    enum class AssetType
    {
        FILE,
        TEXTURE,
        MODEL,
        JSON,
        MATERIAL,
        CURVE,
    };

    struct AssetNode
    {
        std::string name;
        std::string path;
        AssetType type = AssetType::FILE;
        bool isDirectory = false;
        std::vector<AssetNode> children;
    };

    struct AssetBrowserIcons
    {
        std::string folderPath;
        std::string modelPath;
        std::string texturePath;
        std::string jsonPath;
        std::string materialPath;
        std::string curvePath;
        std::string filePath;
        std::string missingPath;
    };

    void Refresh();
    void LoadDirectory(const std::filesystem::path& path_, AssetNode& node_);
    void DrawFolderTree(AssetNode& node_);
    void DrawGrid();
    void DrawAssetCard(AssetNode& node_);
    void BeginAssetDrag(AssetNode& node_);
    void LoadAssetBrowserIcons();
    ID3D11ShaderResourceView* GetAssetIcon(const AssetNode& node_) const;
    ID3D11ShaderResourceView* GetIconTexture(const std::string& path_) const;
    ID3D11ShaderResourceView* GetTextureThumbnail(const AssetNode& node_) const;
    const std::string& GetFallbackIconPath(const std::string& preferredPath_) const;
    const char* GetPayloadType(const AssetNode& node_) const;
    AssetNode* FindNode(const std::string& path_, AssetNode& node_);
    bool MatchesSearch(const AssetNode& node_) const;
    bool HasPathComponent(const std::filesystem::path& path_, const std::string& componentName_) const;
    AssetType ClassifyAsset(const std::filesystem::path& path_) const;

private:
    AssetNode rootNode;
    AssetNode* selectedFolder = nullptr;
    std::string selectedFolderPath;
    std::string selectedFilePath;
    AssetBrowserIcons assetBrowserIcons;
    char searchText[128]{};
    float folderTreeWidth = 240.0f;
    float iconSize = 72.0f;
};
