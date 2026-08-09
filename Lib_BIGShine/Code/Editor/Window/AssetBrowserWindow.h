#pragma once

#include "EditorWindow.h"

#include <d3d11.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class AssetBrowserWindow : public EditorWindow
{
public:
    explicit AssetBrowserWindow(GameObjectManager* manager_);
    void Draw() override;
    void DrawContent();
    void SetPrefabOpenHandler(std::function<void(const std::filesystem::path&)> handler_) { prefabOpenHandler = handler_; }

private:
    enum class AssetType
    {
        // 拡張子から分類した表示/DragDrop用のアセット種別。
        FILE,
        TEXTURE,
        MODEL,
        JSON,
        PREFAB,
        MATERIAL,
        CURVE,
    };

    struct AssetNode
    {
        // Data配下のファイル/フォルダをツリー表示するための軽量ノード。
        std::string name;
        std::string path;
        AssetType type = AssetType::FILE;
        bool isDirectory = false;
        std::vector<AssetNode> children;
    };

    struct AssetBrowserIcons
    {
        // 種別ごとに表示するアイコン画像パス。欠損時はmissingPathへフォールバックする。
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
    // 左のフォルダツリーで選択中のノード。右グリッドの表示元になる。
    AssetNode* selectedFolder = nullptr;
    std::string selectedFolderPath;
    std::string selectedFilePath;
    AssetBrowserIcons assetBrowserIcons;
    std::function<void(const std::filesystem::path&)> prefabOpenHandler;
    char searchText[128]{};
    // フォルダツリー幅とサムネイルサイズはUI操作で調整される表示状態。
    float folderTreeWidth = 240.0f;
    float iconSize = 72.0f;
};
