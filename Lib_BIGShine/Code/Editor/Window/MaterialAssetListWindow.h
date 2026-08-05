#pragma once

#include "EditorWindow.h"

#include <string>
#include <vector>

class MaterialAssetListWindow : public EditorWindow
{
public:
    explicit MaterialAssetListWindow(GameObjectManager* manager_);

    // Material Asset一覧Windowを描画する。
    void Draw() override;

private:
    struct MaterialAssetEntry
    {
        // Drag&Dropや検索に使うAsset識別情報。
        std::string key;
        std::string materialName;
        std::string fileName;
        std::string fullPath;
        std::string baseTexturePath;
        std::string maskTexturePath;
        std::string noiseTexturePath;
    };

    // Asset一覧を再走査してCacheする。
    void Refresh();
    // 検索欄や表示切替などの上部操作を描画する。
    void DrawToolbar();
    // Material一覧をTableとして描画する。
    void DrawMaterialTable();
    // 指定Root配下からMaterialファイルを読み込む。
    void LoadMaterialAssets(const std::string& rootPath_);
    // FullPathからEditor用のAsset Keyを作る。
    std::string BuildAssetKey(
        const std::string& fullPath_,
        const std::string& rootPath_) const;
    // 現在の検索文字列にEntryが一致するか判定する。
    bool MatchesSearch(const MaterialAssetEntry& entry_) const;
    // Materialファイルを読み、一覧表示用Entryへ変換する。
    bool TryReadMaterialEntry(
        const std::string& fullPath_,
        MaterialAssetEntry& entry_) const;
    // Material EntryをDrag&Drop Payloadとして開始する。
    void BeginMaterialDrag(const MaterialAssetEntry& entry_) const;

private:
    // 現在表示中のMaterial Asset Cache。
    std::vector<MaterialAssetEntry> materialAssets;
    // 検索文字列。
    char searchText[128]{};
    // Texture Path列を表示するかどうか。
    bool showTextureColumns = true;
};
