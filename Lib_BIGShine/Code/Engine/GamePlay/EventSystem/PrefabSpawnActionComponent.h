#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/Resource/ModelLoadTypes.h"
#include <memory>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Prefab定義を先に保持し、Actionで生成する設定。
struct PrefabSpawnActionEntry
{
    std::string actionName;
    std::string prefabName;
    std::string actorName;
    bool suspendGameplay = false;
    bool saveSpawnedObjects = false;
    bool showInHierarchy = false; // 保存せずに、本体と子をEditorのObjectリストへ表示する。
};

// Prefabの先読み、Action生成、生成した階層の所有権を管理する。
class PrefabSpawnActionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override;
    // Scene開始前にPrefab定義と共有モデルを準備する。
    void UpdateLoadState() override;
    bool IsLoadReady() const override;
    bool HasRuntimeFailure() const override;
    // 設定済みの生成・戦闘引渡しActionを公開する。
    void Start() override;
    void RegisterActions() override;
    // 所有元の破棄時に、未引渡しの生成物を終了する。
    void OnDestroy() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // コピー済みの定義から生成する。Action時にはPrefabファイルを読み直さない。
    std::shared_ptr<GameObject> SpawnPrefab(const std::string& prefabName_, const std::string& actorName_);
    // 生成済みの個体だけを戦闘へ引き渡し、演出の削除対象から外す。
    void CommitActor(const std::shared_ptr<GameObject>& object_);
    // 削除Componentから呼ばれ、同じ所有元が生成した個体だけを整理する。
    void CleanupSpawnedObjects(bool includeCommitted_ = false);
    // 既存ActionSequenceの生成登録APIとの互換性を保つ。
    void TrackSpawnedObject(const std::shared_ptr<GameObject>& object_);

private:
    struct CachedPrefab
    {
        nlohmann::json definition;
        std::vector<ModelLoadHandle> modelHandles;
    };
    struct SpawnedObject
    {
        std::weak_ptr<GameObject> object;
        bool committed = false;
        std::vector<std::pair<std::weak_ptr<Component>, bool>> suspendedComponents;
    };
    // 未登録の実体を作らず、JSONのコピーとモデル先読みに限定する。
    void PreparePrefabCache();
    // Componentの設定を変えず、生成直後の通常更新と物理を一時停止する。
    void SuspendHierarchy(const std::shared_ptr<GameObject>& object_, SpawnedObject& record_);
    std::vector<PrefabSpawnActionEntry> entries;
    // キャッシュは実行時専用とし、Scene保存にはPrefab名と設定だけを記録する。
    std::vector<CachedPrefab> cachedPrefabs;
    std::vector<SpawnedObject> spawnedObjects;
    std::string commitActionName = "CommitActor";
    std::string commitActorName;
    std::string cacheError;
    std::string lastSpawnStatus = "No spawn action requested";
    bool preloadModels = true;
    bool cachePrepared = false;
};
