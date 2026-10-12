#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include <memory>
#include <cstddef>
#include <string>
#include <vector>

// 対象階層の削除、または同じ所有元が生成した一時Objectの整理を選ぶ。
enum class ObjectDeleteActionType
{
    DELETE_TARGET,
    CLEANUP_SPAWNED
};

// 削除対象とプレビュー時の扱いをActionごとに保持する。
struct ObjectDeleteActionEntry
{
    std::string actionName;
    std::string actorName;
    ObjectDeleteActionType type = ObjectDeleteActionType::DELETE_TARGET;
    bool skipInPreview = false;
    bool includeCommitted = false;
};

// Sequenceからの削除要求をまとめ、Managerの安全な破棄予約へ渡す。
class ObjectDeleteActionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override;
    // 設定済み削除Actionを公開する。
    void Start() override;
    void RegisterActions() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // プレビューの保護条件を確認し、指定した階層の削除を予約する。
    void DeleteActor(const std::string& actorName_, bool skipInPreview_ = false);
    // 名前の再検索を避け、生成記録が指す同一個体の削除を予約する。
    void DeleteObject(const std::shared_ptr<GameObject>& object_, bool skipInPreview_ = false);
    // 同じ所有元の生成Componentへ一時Objectの終了を依頼する。
    void CleanupSpawnedObjects(bool includeCommitted_ = false);

private:
    // 設定された対象削除または生成物整理を実行する。
    void ExecuteEntry(std::size_t index_);
    std::vector<ObjectDeleteActionEntry> entries;
};
