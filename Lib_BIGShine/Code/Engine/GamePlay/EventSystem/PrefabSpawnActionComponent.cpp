#include "PrecompiledHeader.h"
#include "PrefabSpawnActionComponent.h"
#include "ObjectDeleteActionComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <exception>
#include <imgui_stdlib.h>

const std::string& PrefabSpawnActionComponent::GetName() const
{
    static const std::string NAME = "PrefabSpawnActionComponent";
    return NAME;
}

void PrefabSpawnActionComponent::PreparePrefabCache()
{
    BS_PROFILE_SCOPE("PrefabSpawnAction::PreparePrefabCache");
    cachePrepared = true;
    cacheError.clear();
    cachedPrefabs.clear();
    cachedPrefabs.resize(entries.size());
    for (std::size_t index_ = 0; index_ < entries.size(); ++index_)
    {
        const auto& entry_ = entries[index_];
        auto& cache_ = cachedPrefabs[index_];
        try
        {
            const std::string path_ = "Data/Prefabs/" + entry_.prefabName + ".json";
            std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
            if (!file_.is_open()) { cacheError = "Prefab not found: " + path_; break; }
            file_ >> cache_.definition;
            // 単体Prefabも同じ生成経路へ正規化し、既存の保存形式を利用できるようにする。
            if (!cache_.definition.contains("Objects") && cache_.definition.is_object() && cache_.definition.contains("Transform"))
            {
                auto objectDefinition_ = cache_.definition;
                const auto objectName_ = entry_.actorName.empty() ? objectDefinition_.value("Name", entry_.prefabName) : entry_.actorName;
                objectDefinition_["Name"] = objectName_;
                objectDefinition_["ParentName"] = "";
                cache_.definition = {{"RootName",objectName_},{"ObjectList",nlohmann::json::array({objectName_})},
                    {"Objects",{{objectName_,objectDefinition_}}}};
            }
            const auto& definition_ = cache_.definition;
            if (!definition_.contains("Objects") || !definition_.contains("ObjectList") ||
                !definition_["Objects"].is_object() || !definition_["ObjectList"].is_array())
            { cacheError = "Hierarchy prefab required: " + path_; break; }
            const std::string rootName_ = definition_.value("RootName", entry_.prefabName);
            if (!definition_["Objects"].contains(rootName_) || (!entry_.actorName.empty() && entry_.actorName != rootName_))
            { cacheError = "Prefab root name mismatch: " + path_; break; }
            std::set<std::string> names_;
            for (const auto& name_ : definition_["ObjectList"])
            {
                const auto objectName_ = name_.get<std::string>();
                if (!names_.insert(objectName_).second || !definition_["Objects"].contains(objectName_))
                { cacheError = "Invalid prefab object list: " + path_; break; }
            }
            if (!cacheError.empty()) break;
            if (!names_.contains(rootName_)) { cacheError = "Prefab root missing from list: " + path_; break; }
            for (const auto& name_ : names_)
            {
                const auto& object_ = definition_["Objects"][name_];
                if (!object_.is_object() || object_.value("Name", std::string{}) != name_)
                { cacheError = "Prefab object name mismatch: " + path_; break; }
                const auto parentName_ = object_.value("ParentName", std::string{});
                if ((name_ == rootName_ && !parentName_.empty()) ||
                    (name_ != rootName_ && !names_.contains(parentName_)))
                { cacheError = "Invalid prefab parent: " + path_; break; }
                // 親参照の循環を検出し、階層の破棄や生成待ちが終わらなくなるのを防ぐ。
                std::set<std::string> ancestors_;
                auto currentName_ = name_;
                while (!currentName_.empty())
                {
                    if (!ancestors_.insert(currentName_).second) { cacheError = "Cyclic prefab hierarchy: " + path_; break; }
                    currentName_ = definition_["Objects"][currentName_].value("ParentName", std::string{});
                    if (!currentName_.empty() && !names_.contains(currentName_)) break;
                }
                if (!cacheError.empty()) break;
            }
            if (!cacheError.empty()) break;
            if (!preloadModels) continue;
            std::set<std::string> modelPaths_;
            const auto collect_ = [&](auto&& self_, const nlohmann::json& value_) -> void
            {
                if (value_.is_object())
                {
                    for (auto iterator_ = value_.begin(); iterator_ != value_.end(); ++iterator_)
                    {
                        if ((iterator_.key() == "ModelName" || iterator_.key() == "ModelPath") && iterator_.value().is_string())
                        {
                            const auto modelPath_ = iterator_.value().get<std::string>();
                            if (!modelPath_.empty() && !modelPath_.starts_with("__Internal")) modelPaths_.insert(modelPath_);
                        }
                        self_(self_, iterator_.value());
                    }
                }
                else if (value_.is_array()) for (const auto& child_ : value_) self_(self_, child_);
            };
            collect_(collect_, definition_);
            for (const auto& modelPath_ : modelPaths_)
                cache_.modelHandles.push_back(ModelResourceManager::Instance().RequestResourceAsync(modelPath_, static_cast<int>(PRELOAD_PRIORITY::HIGH)));
        }
        catch (const std::exception& exception_)
        {
            cacheError = "Prefab cache error: " + entry_.prefabName + " / " + exception_.what();
            break;
        }
    }
    if (!cacheError.empty()) BS_LOG_ERROR(LOG_CATEGORY::GAME, cacheError);
}

void PrefabSpawnActionComponent::UpdateLoadState()
{
    if (!cachePrepared) PreparePrefabCache();
}

bool PrefabSpawnActionComponent::IsLoadReady() const
{
    if (!cachePrepared || !cacheError.empty()) return false;
    for (const auto& cache_ : cachedPrefabs)
        for (const auto& handle_ : cache_.modelHandles) if (!handle_.IsReady()) return false;
    return true;
}

bool PrefabSpawnActionComponent::HasRuntimeFailure() const
{
    if (!cacheError.empty()) return true;
    for (const auto& cache_ : cachedPrefabs)
        for (const auto& handle_ : cache_.modelHandles) if (!handle_.IsValid() || handle_.IsFailed()) return true;
    return false;
}

void PrefabSpawnActionComponent::Start()
{
    UpdateLoadState();
    RegisterActions();
}

void PrefabSpawnActionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<PrefabSpawnActionComponent> weakSelf_ = owner_->GetComponent<PrefabSpawnActionComponent>();
    for (std::size_t index_ = 0; index_ < entries.size(); ++index_)
    {
        if (entries[index_].actionName.empty()) continue;
        owner_->RegisterGlobalAction("ObjectSpawn", entries[index_].actionName, [weakSelf_, index_]()
        {
            const auto component_ = weakSelf_.lock();
            if (!component_ || index_ >= component_->entries.size()) return;
            const auto entry_ = component_->entries[index_];
            component_->SpawnPrefab(entry_.prefabName, entry_.actorName);
        });
    }
    if (!commitActionName.empty())
        owner_->RegisterGlobalAction("ObjectSpawn", commitActionName, [weakSelf_]()
        {
            const auto component_ = weakSelf_.lock();
            const auto owner_ = component_ ? component_->GetGameObject() : nullptr;
            if (owner_ && owner_->manager) component_->CommitActor(owner_->GetManager().Find(component_->commitActorName));
        });
}

void PrefabSpawnActionComponent::SuspendHierarchy(const std::shared_ptr<GameObject>& object_, SpawnedObject& record_)
{
    object_->SetCombatReady(false);
    for (const auto& component_ : object_->components)
    {
        if (!component_) continue;
        const auto& name_ = component_->GetName();
        if (name_ != "CharacterMovementComponent" && name_ != "MotionWarpComponent" &&
            name_ != "AIControllerComponent" && name_ != "DragonAIComponent" && name_ != "DragonAttackControllerComponent" &&
            name_ != "AIActionComponent" && name_ != "AttackEmitterComponent" && name_ != "HumanBossAIComponent" &&
            name_ != "HumanBossActionComponent") continue;
        record_.suspendedComponents.push_back({component_, component_->IsRuntimeSuppressed()});
        component_->SetRuntimeSuppressed(true);
    }
    for (const auto& component_ : object_->colliderComponents)
    {
        if (!component_) continue;
        record_.suspendedComponents.push_back({component_, component_->IsRuntimeSuppressed()});
        component_->SetRuntimeSuppressed(true);
        // 読込時に用意された物理実体も取り外し、Start後の解除処理と合わせて停止を保つ。
        if (const auto collider_ = std::dynamic_pointer_cast<ColliderComponent>(component_))
        {
            collider_->velocity = {0.0f,0.0f,0.0f};
            collider_->currentTriggers.clear();
            collider_->m_CollisionMainFolds.clear();
            collider_->ReleasePhysXActor();
        }
    }
    for (const auto& child_ : object_->transform->children)
        if (child_) if (const auto actor_ = child_->gameObject.lock()) SuspendHierarchy(actor_, record_);
}

std::shared_ptr<GameObject> PrefabSpawnActionComponent::SpawnPrefab(const std::string& prefabName_, const std::string& actorName_)
{
    BS_PROFILE_SCOPE("PrefabSpawnAction::SpawnPrefab");
    const auto owner_ = GetGameObject();
    lastSpawnStatus = "Spawn requested: " + prefabName_;
    if (!owner_ || !owner_->manager || owner_->IsPendingDestroy()) { lastSpawnStatus = "Spawn failed: owner is not registered"; BS_LOG_WARNING(LOG_CATEGORY::GAME, lastSpawnStatus); return nullptr; }
    if (!IsLoadReady())
    {
        lastSpawnStatus = "Spawn blocked: prefab cache not ready / " + cacheError;
        BS_LOG_WARNING(LOG_CATEGORY::GAME, lastSpawnStatus);
        return nullptr;
    }
    const auto entry_ = std::find_if(entries.begin(), entries.end(), [&](const PrefabSpawnActionEntry& entry_)
        { return entry_.prefabName == prefabName_ && entry_.actorName == actorName_; });
    if (entry_ == entries.end()) { lastSpawnStatus = "Spawn failed: entry not registered / " + prefabName_; BS_LOG_WARNING(LOG_CATEGORY::GAME, lastSpawnStatus); return nullptr; }
    const auto index_ = static_cast<std::size_t>(std::distance(entries.begin(), entry_));
    // 名前参照を維持するため、同名の階層を重複生成しない。
    const auto& definition_ = cachedPrefabs[index_].definition;
    const auto rootName_ = definition_.value("RootName", prefabName_);
    if (const auto existing_ = owner_->GetManager().Find(rootName_))
    {
        lastSpawnStatus = "Existing object: " + rootName_;
        return existing_;
    }
    for (const auto& name_ : definition_["ObjectList"])
        if (owner_->GetManager().Find(name_.get<std::string>()))
        { lastSpawnStatus = "Spawn failed: child name already exists / " + name_.get<std::string>(); BS_LOG_WARNING(LOG_CATEGORY::GAME, lastSpawnStatus); return nullptr; }
    std::vector<std::shared_ptr<GameObject>> created_;
    std::shared_ptr<GameObject> root_;
    try
    {
        for (const auto& name_ : definition_["ObjectList"])
        {
            const auto objectName_ = name_.get<std::string>();
            const auto object_ = owner_->GetManager().Create(objectName_);
            created_.push_back(object_);
            // 保存済み定義をコピーし、Prefabのキャッシュ自体を変更しない。
            const nlohmann::json objectDefinition_ = definition_["Objects"][objectName_];
            object_->Load(objectDefinition_);
            object_->saveFlg = entry_->saveSpawnedObjects;
            object_->SetRuntimeHierarchyVisible(entry_->showInHierarchy);
            if (objectName_ == rootName_) root_ = object_;
        }
        owner_->GetManager().ResolveHierarchy();
        SpawnedObject record_;
        record_.object = root_;
        if (entry_->suspendGameplay) SuspendHierarchy(root_, record_);
        spawnedObjects.push_back(std::move(record_));
    }
    catch (const std::exception& exception_)
    {
        // 一部の実体化が失敗しても、作成途中のObjectをSceneに残さない。
        for (const auto& object_ : created_) owner_->GetManager().Destroy(object_);
        lastSpawnStatus = std::string("Prefab spawn failed: ") + exception_.what();
        BS_LOG_ERROR(LOG_CATEGORY::GAME, lastSpawnStatus);
        return nullptr;
    }
    lastSpawnStatus = "Object registered: " + rootName_ + " / hierarchy size=" + std::to_string(created_.size());
    BS_LOG_INFO(LOG_CATEGORY::GAME, lastSpawnStatus);
    return root_;
}

void PrefabSpawnActionComponent::TrackSpawnedObject(const std::shared_ptr<GameObject>& object_)
{
    if (!object_) return;
    const auto duplicate_ = std::find_if(spawnedObjects.begin(), spawnedObjects.end(),
        [&](const SpawnedObject& entry_) { return entry_.object.lock() == object_; });
    if (duplicate_ == spawnedObjects.end()) { SpawnedObject record_; record_.object = object_; spawnedObjects.push_back(std::move(record_)); }
}

void PrefabSpawnActionComponent::CommitActor(const std::shared_ptr<GameObject>& object_)
{
    BS_PROFILE_SCOPE("PrefabSpawnAction::CommitActor");
    if (!object_ || object_->IsPendingDestroy() || !object_->IsHierarchyReady()) return;
    const auto record_ = std::find_if(spawnedObjects.begin(), spawnedObjects.end(),
        [&](const SpawnedObject& entry_) { return entry_.object.lock() == object_; });
    if (record_ == spawnedObjects.end() || record_->committed) return;
    for (const auto& state_ : record_->suspendedComponents)
        if (const auto component_ = state_.first.lock())
        {
            component_->SetRuntimeSuppressed(state_.second);
            if (const auto hitBox_ = std::dynamic_pointer_cast<HitBox>(component_)) hitBox_->Deactivate();
        }
    const auto activate_ = [&](auto&& self_, const std::shared_ptr<GameObject>& actor_) -> void
    {
        actor_->SetCombatReady(true);
        for (const auto& child_ : actor_->transform->children) if (child_) if (const auto object_ = child_->gameObject.lock()) self_(self_, object_);
    };
    activate_(activate_, object_);
    record_->suspendedComponents.clear();
    record_->committed = true;
}

void PrefabSpawnActionComponent::CleanupSpawnedObjects(bool includeCommitted_)
{
    BS_PROFILE_SCOPE("PrefabSpawnAction::CleanupSpawnedObjects");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    const auto deleter_ = owner_->GetComponent<ObjectDeleteActionComponent>();
    const auto retiring_ = std::move(spawnedObjects);
    spawnedObjects.clear();
    for (const auto& record_ : retiring_)
        if (includeCommitted_ || !record_.committed)
            if (const auto object_ = record_.object.lock())
            {
                if (deleter_) deleter_->DeleteObject(object_);
                else owner_->GetManager().Destroy(object_);
            }
}

void PrefabSpawnActionComponent::OnDestroy()
{
    CleanupSpawnedObjects(true);
}

void PrefabSpawnActionComponent::Gui()
{
    BS_PROFILE_SCOPE("PrefabSpawnActionComponent::Gui");
    if (ImGui::Checkbox("Preload Models", &preloadModels)) cachePrepared = false;
    if (ImGui::Button("Reload Prefab Copies")) PreparePrefabCache();
    if (!cacheError.empty()) ImGui::TextWrapped("%s", cacheError.c_str());
    ImGui::Text("Cache Ready: %s", IsLoadReady() ? "yes" : "no");
    ImGui::TextWrapped("%s", lastSpawnStatus.c_str());
    ImGui::InputText("Commit Action", &commitActionName);
    ImGui::InputText("Commit Actor", &commitActorName);
    for (std::size_t index_ = 0; index_ < entries.size(); ++index_)
    {
        ImGui::PushID(static_cast<int>(index_));
        auto& entry_ = entries[index_];
        bool changed_ = ImGui::InputText("Spawn Action", &entry_.actionName);
        changed_ |= ImGui::InputText("Prefab Name", &entry_.prefabName);
        changed_ |= ImGui::InputText("Prefab Root Name", &entry_.actorName);
        changed_ |= ImGui::Checkbox("Suspend Gameplay", &entry_.suspendGameplay);
        changed_ |= ImGui::Checkbox("Save Spawned Objects", &entry_.saveSpawnedObjects);
        const bool visibilityChanged_ = ImGui::Checkbox("Show In Hierarchy", &entry_.showInHierarchy);
        if (changed_) cachePrepared = false;
        if (ImGui::Button("Spawn")) SpawnPrefab(entry_.prefabName, entry_.actorName);
        const auto owner_ = GetGameObject();
        const auto object_ = owner_ && owner_->manager ? owner_->GetManager().Find(entry_.actorName) : nullptr;
        ImGui::Text("Object registered: %s", object_ ? "yes" : "no");
        if (object_)
        {
            if (visibilityChanged_)
            {
                const auto show_ = [&](auto&& self_, const std::shared_ptr<GameObject>& actor_) -> void
                {
                    actor_->SetRuntimeHierarchyVisible(entry_.showInHierarchy);
                    for (const auto& child_ : actor_->transform->children) if (child_) if (const auto childActor_ = child_->gameObject.lock()) self_(self_, childActor_);
                };
                show_(show_, object_);
            }
            ImGui::Text("Hierarchy ready: %s / Combat ready: %s", object_->IsHierarchyReady() ? "yes" : "no", object_->IsCombatReady() ? "yes" : "no");
            bool hasAi_ = false;
            for (const auto& component_ : object_->components)
                if (component_ && component_->GetName().find("AI") != std::string::npos)
                {
                    hasAi_ = true;
                    ImGui::Text("%s: started=%s / enabled=%s", component_->GetName().c_str(),
                        component_->IsStarted() ? "yes" : "no", component_->IsEnabled() ? "yes" : "no");
                }
            if (!hasAi_) ImGui::TextDisabled("AI component: none");
            if (ImGui::Button("Select Spawned Object")) owner_->GetManager().SetSelectedObject(object_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove")) { entries.erase(entries.begin() + index_); cachePrepared = false; ImGui::PopID(); break; }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button("Add Spawn Action")) { entries.emplace_back(); cachePrepared = false; }
    if (ImGui::Button("Register Spawn Actions")) RegisterActions();
}

void PrefabSpawnActionComponent::Serialize(nlohmann::json& json_) const
{
    json_["PreloadModels"] = preloadModels;
    json_["CommitActionName"] = commitActionName;
    json_["CommitActorName"] = commitActorName;
    json_["Entries"] = nlohmann::json::array();
    for (const auto& entry_ : entries)
        json_["Entries"].push_back({{"ActionName",entry_.actionName},{"PrefabName",entry_.prefabName},
            {"ActorName",entry_.actorName},{"SuspendGameplay",entry_.suspendGameplay},{"SaveSpawnedObjects",entry_.saveSpawnedObjects},{"ShowInHierarchy",entry_.showInHierarchy}});
}

void PrefabSpawnActionComponent::Deserialize(const nlohmann::json& json_)
{
    preloadModels = json_.value("PreloadModels", true);
    commitActionName = json_.value("CommitActionName", std::string("CommitActor"));
    commitActorName = json_.value("CommitActorName", std::string{});
    entries.clear();
    for (const auto& item_ : json_.value("Entries", nlohmann::json::array()))
    {
        PrefabSpawnActionEntry entry_;
        entry_.actionName = item_.value("ActionName", std::string{});
        entry_.prefabName = item_.value("PrefabName", std::string{});
        entry_.actorName = item_.value("ActorName", std::string{});
        entry_.suspendGameplay = item_.value("SuspendGameplay", false);
        entry_.saveSpawnedObjects = item_.value("SaveSpawnedObjects", false);
        entry_.showInHierarchy = item_.value("ShowInHierarchy", false);
        entries.push_back(entry_);
    }
    cachePrepared = false;
    cachedPrefabs.clear();
    cacheError.clear();
}

REGISTER_COMPONENT(PrefabSpawnActionComponent, "PrefabSpawnActionComponent")
