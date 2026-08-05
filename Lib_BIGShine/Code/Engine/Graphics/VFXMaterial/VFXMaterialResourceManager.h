#pragma once

#include "VFXMaterialResource.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class VFXMaterialResourceManager
{
public:
    // VFX Materialを共有管理するSingleton。
    static VFXMaterialResourceManager& GetInstance();

    // ファイルからMaterialを読み込み、同じPathはCacheから再利用する。
    std::shared_ptr<VFXMaterialResource> Load(const std::string& filePath_);
    // 読み込み済みMaterialだけを探す。見つからない場合はnullptrを返す。
    std::shared_ptr<VFXMaterialResource> Find(const std::string& filePath_) const;
    // 読み込み失敗時に使うFallback Materialを取得する。
    std::shared_ptr<VFXMaterialResource> GetDefaultMaterial();
    // Editor Previewなど、ファイル保存前の一時Materialを作る。
    std::shared_ptr<VFXMaterialResource> CreateTemporary(const VFXMaterialData& materialData_);

    // 既存Resourceの中身をファイルから読み直す。
    bool Reload(const std::string& filePath_);
    // MaterialDataをファイルへ保存し、ManagerのCacheも同じResourceへ差し替える。
    bool Save(
        const std::string& filePath_,
        const std::shared_ptr<VFXMaterialResource>& materialResource_);
    // 外部で作成したResourceを指定PathのCacheとして登録する。
    void RegisterOrReplace(
        const std::string& filePath_,
        const std::shared_ptr<VFXMaterialResource>& materialResource_);
    void Unload(const std::string& filePath_);
    void Clear();

    std::string NormalizePath(const std::string& filePath_) const;

private:
    // 同じ失敗LogをFrameごとに出し続けないよう、一度だけ表示する。
    void LogLoadFailureOnce(
        const std::string& filePath_,
        const char* reason_) const;

private:
    // 正規化PathをKeyにしたMaterial Cache。
    std::unordered_map<std::string, std::shared_ptr<VFXMaterialResource>> materialResources;
    // 既に出力した読み込み失敗Logの集合。
    mutable std::unordered_set<std::string> loggedLoadFailures;
    // 読み込みに失敗したMaterialの代わりに使う共有Fallback。
    std::shared_ptr<VFXMaterialResource> defaultMaterial;
};
