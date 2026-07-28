#pragma once

#include "EditorWindow.h"

#include <string>
#include <vector>

class MaterialAssetListWindow : public EditorWindow
{
public:
    explicit MaterialAssetListWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    struct MaterialAssetEntry
    {
        std::string key;
        std::string materialName;
        std::string fileName;
        std::string fullPath;
        std::string baseTexturePath;
        std::string maskTexturePath;
        std::string noiseTexturePath;
    };

    void Refresh();
    void DrawToolbar();
    void DrawMaterialTable();
    void LoadMaterialAssets(const std::string& rootPath_);
    std::string BuildAssetKey(
        const std::string& fullPath_,
        const std::string& rootPath_) const;
    bool MatchesSearch(const MaterialAssetEntry& entry_) const;
    bool TryReadMaterialEntry(
        const std::string& fullPath_,
        MaterialAssetEntry& entry_) const;
    void BeginMaterialDrag(const MaterialAssetEntry& entry_) const;

private:
    std::vector<MaterialAssetEntry> materialAssets;
    char searchText[128]{};
    bool showTextureColumns = true;
};