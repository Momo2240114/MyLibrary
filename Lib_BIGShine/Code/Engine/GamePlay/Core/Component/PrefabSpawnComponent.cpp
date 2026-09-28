#include "PrecompiledHeader.h"

#include "PrefabSpawnComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    DirectX::XMFLOAT3 ReadFloat3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 3)
        {
            return fallback_;
        }

        return {
            json_[key_][0].get<float>(),
            json_[key_][1].get<float>(),
            json_[key_][2].get<float>()
        };
    }

    void WriteFloat3(
        nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& value_)
    {
        json_[key_] = { value_.x, value_.y, value_.z };
    }
}

const std::string& PrefabSpawnComponent::GetName() const
{
    static const std::string name_ = "PrefabSpawnComponent";
    return name_;
}

void PrefabSpawnComponent::Start()
{
    BS_PROFILE_SCOPE("PrefabSpawnComponent::Start");

    hasSpawned = false;
}

void PrefabSpawnComponent::Update(float deltaTime_)
{
    (void)deltaTime_;
    BS_PROFILE_SCOPE("PrefabSpawnComponent::Update");

    if (!spawnOnStart || hasSpawned) return;

    hasSpawned = true;
    for (const SpawnEntry& entry_ : entries)
    {
        SpawnEntryNow(entry_);
    }
}

void PrefabSpawnComponent::Gui()
{
    BS_PROFILE_SCOPE("PrefabSpawnComponent::Gui");

    ImGui::Checkbox("Spawn On Start", &spawnOnStart);
    if (ImGui::Button("Add Spawn Entry"))
    {
        entries.emplace_back();
    }

    for (std::size_t index_ = 0; index_ < entries.size(); ++index_)
    {
        SpawnEntry& entry_ = entries[index_];
        const std::string header_ = "Spawn Entry " + std::to_string(index_);
        if (ImGui::TreeNode(header_.c_str()))
        {
            ImGui::InputText("Prefab Name", &entry_.prefabName);
            ImGui::Checkbox("Enabled", &entry_.enabled);
            ImGui::Checkbox("Select After Spawn", &entry_.selectAfterSpawn);
            ImGui::DragFloat3("Position", &entry_.position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &entry_.rotation.x, 0.5f);
            ImGui::DragFloat3("Scale", &entry_.scale.x, 0.01f, 0.01f);

            if (ImGui::Button("Spawn Now"))
            {
                SpawnEntryNow(entry_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                entries.erase(entries.begin() + static_cast<std::vector<SpawnEntry>::difference_type>(index_));
                ImGui::TreePop();
                break;
            }

            ImGui::TreePop();
        }
    }
}

void PrefabSpawnComponent::Serialize(nlohmann::json& json_) const
{
    json_["ComponentName"] = GetName();
    json_["SpawnOnStart"] = spawnOnStart;
    json_["Entries"] = nlohmann::json::array();

    for (const SpawnEntry& entry_ : entries)
    {
        nlohmann::json entryJson_;
        entryJson_["PrefabName"] = entry_.prefabName;
        entryJson_["Enabled"] = entry_.enabled;
        entryJson_["SelectAfterSpawn"] = entry_.selectAfterSpawn;
        WriteFloat3(entryJson_, "Position", entry_.position);
        WriteFloat3(entryJson_, "Rotation", entry_.rotation);
        WriteFloat3(entryJson_, "Scale", entry_.scale);
        json_["Entries"].push_back(entryJson_);
    }
}

void PrefabSpawnComponent::Deserialize(const nlohmann::json& json_)
{
    spawnOnStart = json_.value("SpawnOnStart", true);
    hasSpawned = false;
    entries.clear();

    if (!json_.contains("Entries") || !json_["Entries"].is_array()) return;

    for (const nlohmann::json& entryJson_ : json_["Entries"])
    {
        SpawnEntry entry_;
        entry_.prefabName = entryJson_.value("PrefabName", std::string());
        entry_.enabled = entryJson_.value("Enabled", true);
        entry_.selectAfterSpawn = entryJson_.value("SelectAfterSpawn", false);
        entry_.position = ReadFloat3(entryJson_, "Position", { 0.0f, 0.0f, 0.0f });
        entry_.rotation = ReadFloat3(entryJson_, "Rotation", { 0.0f, 0.0f, 0.0f });
        entry_.scale = ReadFloat3(entryJson_, "Scale", { 1.0f, 1.0f, 1.0f });
        entries.push_back(entry_);
    }
}

void PrefabSpawnComponent::SpawnEntryNow(const SpawnEntry& entry_)
{
    BS_PROFILE_SCOPE("PrefabSpawnComponent::SpawnEntryNow");

    if (!entry_.enabled || entry_.prefabName.empty()) return;

    std::shared_ptr<GameObject> spawnedObject_ = GetManager().Instantiate(entry_.prefabName);
    if (!spawnedObject_ || !spawnedObject_->transform) return;

    // Prefab読込後に、シーン配置用の初期Transformで上書きする。
    spawnedObject_->transform->localPosition = entry_.position;
    spawnedObject_->transform->localRotation = entry_.rotation;
    spawnedObject_->transform->localScale = entry_.scale;
    spawnedObject_->transform->MarkDirty();

    if (entry_.selectAfterSpawn)
    {
        GetManager().SetSelectedObject(spawnedObject_);
    }
}

REGISTER_COMPONENT(PrefabSpawnComponent, "PrefabSpawnComponent")
