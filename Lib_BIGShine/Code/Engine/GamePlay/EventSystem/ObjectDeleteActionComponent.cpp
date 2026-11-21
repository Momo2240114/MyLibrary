#include "PrecompiledHeader.h"
#include "ObjectDeleteActionComponent.h"
#include "PrefabSpawnActionComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Game/GameMode/GameModeManager.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include <imgui_stdlib.h>

const std::string& ObjectDeleteActionComponent::GetName() const
{
    static const std::string NAME = "ObjectDeleteActionComponent";
    return NAME;
}

void ObjectDeleteActionComponent::Start()
{
    RegisterActions();
}

void ObjectDeleteActionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    const std::weak_ptr<ObjectDeleteActionComponent> weakSelf_ = owner_->GetComponent<ObjectDeleteActionComponent>();
    for (std::size_t index_ = 0; index_ < entries.size(); ++index_)
    {
        if (!entries[index_].actionName.empty())
        {
            owner_->RegisterGlobalAction("ObjectDelete", entries[index_].actionName, [weakSelf_, index_]()
            {
                if (const auto component_ = weakSelf_.lock())
                {
                    component_->ExecuteEntry(index_);
                }
            });
        }
    }
}

void ObjectDeleteActionComponent::DeleteActor(const std::string& actorName_, bool skipInPreview_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager)
    {
        return;
    }
    DeleteObject(owner_->GetManager().Find(actorName_), skipInPreview_);
}

void ObjectDeleteActionComponent::DeleteObject(const std::shared_ptr<GameObject>& object_, bool skipInPreview_)
{
    BS_PROFILE_SCOPE("ObjectDeleteAction::DeleteObject");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !object_ || object_->IsPendingDestroy() || object_->manager != owner_->manager)
    {
        return;
    }
    const auto mode_ = owner_->GetManager().GetGameModeManager();
    if (skipInPreview_ && mode_ && mode_->IsPhaseTransitionPreview())
    {
        return;
    }
    // 演出対象の選択解除は削除Actionが担当し、Managerへ戦闘固有の型を持ち込まない。
    const auto cameras_ = owner_->GetManager().FindComponents<CameraLockOnComponent>();
    const auto releaseTargets_ = [&](auto&& self_, const std::shared_ptr<GameObject>& actor_) -> void
    {
        for (const auto& camera_ : cameras_)
        {
            if (camera_)
            {
                camera_->ForgetObject(actor_);
            }
        }
        for (const auto& child_ : actor_->transform->children)
        {
            if (child_)
            {
                if (const auto childObject_ = child_->gameObject.lock())
                {
                    self_(self_, childObject_);
                }
            }
        }
    };
    releaseTargets_(releaseTargets_, object_);
    // 演出による破棄は配置そのものの削除と分け、途中でScene保存しても初期配置を失わない。
    owner_->GetManager().DestroyRuntimeObject(object_);
}

void ObjectDeleteActionComponent::CleanupSpawnedObjects(bool includeCommitted_)
{
    BS_PROFILE_SCOPE("ObjectDeleteAction::CleanupSpawnedObjects");
    const auto owner_ = GetGameObject();
    const auto spawner_ = owner_ ? owner_->GetComponent<PrefabSpawnActionComponent>() : nullptr;
    if (spawner_)
    {
        spawner_->CleanupSpawnedObjects(includeCommitted_);
    }
}

void ObjectDeleteActionComponent::ExecuteEntry(std::size_t index_)
{
    if (index_ >= entries.size())
    {
        return;
    }
    const auto entry_ = entries[index_];
    const auto owner_ = GetGameObject();
    const auto mode_ = owner_ && owner_->manager ? owner_->GetManager().GetGameModeManager() : nullptr;
    if (entry_.skipInPreview && mode_ && mode_->IsPhaseTransitionPreview())
    {
        return;
    }
    if (entry_.type == ObjectDeleteActionType::CLEANUP_SPAWNED)
    {
        CleanupSpawnedObjects(entry_.includeCommitted);
    }
    else
    {
        DeleteActor(entry_.actorName, entry_.skipInPreview);
    }
}

void ObjectDeleteActionComponent::Gui()
{
    const char* TYPES[] = {"Delete Target Hierarchy", "Cleanup Spawned Objects"};
    for (std::size_t index_ = 0; index_ < entries.size(); ++index_)
    {
        ImGui::PushID(static_cast<int>(index_));
        auto& entry_ = entries[index_];
        ImGui::InputText("Delete Action", &entry_.actionName);
        ImGui::InputText("Target Object", &entry_.actorName);
        int type_ = static_cast<int>(entry_.type);
        if (ImGui::Combo("Type", &type_, TYPES, IM_ARRAYSIZE(TYPES)))
        {
            entry_.type = static_cast<ObjectDeleteActionType>(type_);
        }
        ImGui::Checkbox("Skip In Preview", &entry_.skipInPreview);
        ImGui::Checkbox("Include Committed Objects", &entry_.includeCommitted);
        if (ImGui::Button("Remove"))
        {
            entries.erase(entries.begin() + index_);
            ImGui::PopID();
            break;
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button("Add Delete Action"))
    {
        entries.emplace_back();
    }
    if (ImGui::Button("Register Delete Actions"))
    {
        RegisterActions();
    }
}

void ObjectDeleteActionComponent::Serialize(nlohmann::json& json_) const
{
    json_["Entries"] = nlohmann::json::array();
    for (const auto& entry_ : entries)
    {
        json_["Entries"].push_back({{"ActionName",entry_.actionName},{"ActorName",entry_.actorName},
            {"Type",static_cast<int>(entry_.type)},{"SkipInPreview",entry_.skipInPreview},{"IncludeCommitted",entry_.includeCommitted}});
    }
}

void ObjectDeleteActionComponent::Deserialize(const nlohmann::json& json_)
{
    entries.clear();
    for (const auto& item_ : json_.value("Entries", nlohmann::json::array()))
    {
        ObjectDeleteActionEntry entry_;
        entry_.actionName = item_.value("ActionName", std::string{});
        entry_.actorName = item_.value("ActorName", std::string{});
        entry_.type = item_.value("Type", 0) == 1 ? ObjectDeleteActionType::CLEANUP_SPAWNED : ObjectDeleteActionType::DELETE_TARGET;
        entry_.skipInPreview = item_.value("SkipInPreview", false);
        entry_.includeCommitted = item_.value("IncludeCommitted", false);
        entries.push_back(entry_);
    }
}

REGISTER_COMPONENT(ObjectDeleteActionComponent, "ObjectDeleteActionComponent")
