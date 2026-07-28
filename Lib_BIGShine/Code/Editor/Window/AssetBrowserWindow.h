#pragma once

#include "EditorWindow.h"

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

    void Refresh();
    void LoadDirectory(const std::filesystem::path& path_, AssetNode& node_);
    void DrawFolderTree(AssetNode& node_);
    void DrawGrid();
    void DrawAssetCard(AssetNode& node_);
    void BeginAssetDrag(AssetNode& node_);
    AssetNode* FindNode(const std::string& path_, AssetNode& node_);
    bool MatchesSearch(const AssetNode& node_) const;
    AssetType ClassifyAsset(const std::filesystem::path& path_) const;

private:
    AssetNode rootNode;
    AssetNode* selectedFolder = nullptr;
    std::string selectedFolderPath;
    std::string selectedFilePath;
    char searchText[128]{};
    float folderTreeWidth = 240.0f;
    float iconSize = 72.0f;
};
