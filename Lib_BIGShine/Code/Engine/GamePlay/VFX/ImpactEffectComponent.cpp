#include "ImpactEffectComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Physics/ColliderComponent.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    DirectX::XMFLOAT3 ReadVector3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_)
    {
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 3u) return fallback_;
        return { value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
    }

    DirectX::XMFLOAT4 ReadVector4(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT4& fallback_)
    {
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 4u) return fallback_;
        return {
            value_[0].get<float>(), value_[1].get<float>(),
            value_[2].get<float>(), value_[3].get<float>() };
    }
}

void to_json(nlohmann::json& json_, const ImpactEffectEntry& entry_)
{
    json_ = {
        { "Type", static_cast<int>(entry_.type) },
        { "Enabled", entry_.enabled },
        { "AssetPath", entry_.assetPath },
        { "EmitterName", entry_.emitterName },
        { "Scale", { entry_.scale.x, entry_.scale.y, entry_.scale.z } },
        { "Color", { entry_.color.x, entry_.color.y, entry_.color.z, entry_.color.w } },
        { "LifeTime", entry_.lifeTime },
        { "FadeOutDuration", entry_.fadeOutDuration },
        { "NormalOffset", entry_.normalOffset },
    };
}

void from_json(const nlohmann::json& json_, ImpactEffectEntry& entry_)
{
    entry_.type = static_cast<IMPACT_EFFECT_ENTRY_TYPE>((std::clamp)(json_.value("Type", 0), 0, 2));
    entry_.enabled = json_.value("Enabled", true);
    entry_.assetPath = json_.value("AssetPath", std::string{});
    entry_.emitterName = json_.value("EmitterName", std::string{});
    entry_.scale = ReadVector3(json_, "Scale", entry_.scale);
    entry_.color = ReadVector4(json_, "Color", entry_.color);
    entry_.lifeTime = (std::max)(json_.value("LifeTime", 1.0f), 0.01f);
    entry_.fadeOutDuration = (std::clamp)(
        json_.value("FadeOutDuration", 0.0f), 0.0f, entry_.lifeTime);
    entry_.normalOffset = json_.value("NormalOffset", 0.0f);
}

void to_json(nlohmann::json& json_, const ImpactEffectAsset& asset_)
{
    json_["AssetType"] = "ImpactEffect";
    json_["AssetVersion"] = 1;
    json_["Entries"] = asset_.entries;
}

void from_json(const nlohmann::json& json_, ImpactEffectAsset& asset_)
{
    asset_.entries = json_.value("Entries", std::vector<ImpactEffectEntry>{});
}

void ImpactEffectComponent::Start()
{
    BindColliders();
}

bool ImpactEffectComponent::LoadAsset(const std::string& path_)
{
    if (path_.empty()) return false;
    std::ifstream file_(path_);
    if (!file_.is_open()) return false;
    nlohmann::json json_;
    try
    {
        file_ >> json_;
        if (json_.value("AssetType", std::string{}) != "ImpactEffect") return false;
        asset = json_.get<ImpactEffectAsset>();
        assetPath = path_;
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ImpactEffectComponent::SaveAsset(const std::string& path_) const
{
    if (path_.empty()) return false;
    const std::filesystem::path outputPath_(path_);
    std::filesystem::create_directories(outputPath_.parent_path());
    std::ofstream file_(outputPath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;
    const nlohmann::json json_ = asset;
    file_ << json_.dump(4);
    return file_.good();
}

void ImpactEffectComponent::Update(float deltaTime_)
{
    coolDownTimer = (std::max)(coolDownTimer - (std::max)(deltaTime_, 0.0f), 0.0f);
    BindColliders();
}

void ImpactEffectComponent::BindColliders()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    for (const std::shared_ptr<ColliderComponent>& collider_ :
        owner_->GetComponents<ColliderComponent>())
    {
        if (!collider_ || !boundColliders.insert(collider_.get()).second) continue;
        collider_->onCollisionContactCallbacks.push_back(
            [this](std::shared_ptr<GameObject> other_, const CollisionContact& contact_)
            {
                OnCollision(other_, contact_);
            });
        collider_->onTriggerEnterCallbacks.push_back(
            [this](std::shared_ptr<GameObject> other_)
            {
                OnTrigger(other_);
            });
    }
}

bool ImpactEffectComponent::CanSpawnFor(
    const std::shared_ptr<GameObject>& other_) const
{
    if (!IsEnabled() || !other_ || coolDownTimer > 0.0f) return false;
    if (spawnOnlyOnce && hasSpawned) return false;
    return (targetLayerMask & other_->GetLayerBit()) != 0u;
}

void ImpactEffectComponent::OnCollision(
    const std::shared_ptr<GameObject>& other_,
    const CollisionContact& contact_)
{
    if (!CanSpawnFor(other_)) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;
    const DirectX::XMFLOAT3 position_ = contact_.isValid
        ? contact_.position
        : owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 normal_ = contact_.isValid
        ? contact_.normal
        : DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
    SpawnAt(position_, normal_);
}

void ImpactEffectComponent::OnTrigger(
    const std::shared_ptr<GameObject>& other_)
{
    if (!listenToTriggers || !CanSpawnFor(other_)) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform || !other_->transform) return;

    const DirectX::XMFLOAT3 position_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 otherPosition_ = other_->transform->GetWorldPosition();
    DirectX::XMFLOAT3 normal_ = {
        position_.x - otherPosition_.x,
        position_.y - otherPosition_.y,
        position_.z - otherPosition_.z };
    const DirectX::XMVECTOR normalVector_ = DirectX::XMLoadFloat3(&normal_);
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normalVector_)) > 0.000001f)
    {
        DirectX::XMStoreFloat3(&normal_, DirectX::XMVector3Normalize(normalVector_));
    }
    else
    {
        normal_ = { 0.0f, 1.0f, 0.0f };
    }
    SpawnAt(position_, normal_);
}

void ImpactEffectComponent::SpawnAt(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& normal_)
{
    EffectManager& effectManager_ = GetManager().GetEffectManager();
    if (!assetPath.empty())
    {
        effectManager_.SpawnImpactEffectAsset(assetPath, position_, normal_);
        coolDownTimer = (std::max)(coolDown, 0.0f);
        hasSpawned = true;
        return;
    }
    for (const ImpactEffectEntry& entry_ : asset.entries)
    {
        if (!entry_.enabled || entry_.assetPath.empty()) continue;

        ImpactEffectSpawnRequest request_{};
        request_.position = position_;
        request_.normal = normal_;
        if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::PARTICLE)
        {
            request_.spawnParticle = true;
            request_.particle.assetPath = entry_.assetPath;
            request_.particle.emitterName = entry_.emitterName;
            request_.particle.lifeTime = entry_.lifeTime;
            request_.particle.normalOffset = entry_.normalOffset;
        }
        else if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::MESH_VFX)
        {
            request_.spawnMeshVFX = true;
            request_.meshVFX.assetPath = entry_.assetPath;
            request_.meshVFX.lifeTime = entry_.lifeTime;
            request_.meshVFX.normalOffset = entry_.normalOffset;
        }
        else if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::DECAL)
        {
            request_.spawnDecal = true;
            request_.decal.texturePath = entry_.assetPath;
            request_.decal.scale = entry_.scale;
            request_.decal.color = entry_.color;
            request_.decal.lifeTime = entry_.lifeTime;
            request_.decal.fadeOutDuration = entry_.fadeOutDuration;
            request_.decal.normalOffset = entry_.normalOffset;
        }
        effectManager_.SpawnImpactEffect(request_);
    }
    coolDownTimer = (std::max)(coolDown, 0.0f);
    hasSpawned = true;
}

void ImpactEffectComponent::Gui()
{
    std::string selectedAssetPath_ = assetPath;
    if (AssetSlotGui::Draw(
        "Impact Effect Asset",
        AssetSlotType::JSON,
        selectedAssetPath_,
        nullptr,
        "Data/ImpactEffect"))
    {
        if (selectedAssetPath_.empty()) assetPath.clear();
        else LoadAsset(selectedAssetPath_);
    }
    if (!assetPath.empty())
    {
        if (ImGui::Button("Reload Impact Asset")) LoadAsset(assetPath);
        ImGui::SameLine();
        if (ImGui::Button("Save Impact Asset")) SaveAsset(assetPath);
    }
    ImGui::Separator();
    ImGui::Checkbox("Spawn Only Once", &spawnOnlyOnce);
    ImGui::Checkbox("Listen To Triggers", &listenToTriggers);
    ImGui::DragFloat("Cool Down", &coolDown, 0.01f, 0.0f, 10.0f);
    ImGui::InputScalar("Target Layer Mask", ImGuiDataType_U32, &targetLayerMask);

    static const char* typeNames_[] = { "Particle", "Mesh VFX / Sequence", "Decal" };
    int removeIndex_ = -1;
    for (int index_ = 0; index_ < static_cast<int>(asset.entries.size()); ++index_)
    {
        ImpactEffectEntry& entry_ = asset.entries[index_];
        ImGui::PushID(index_);
        if (ImGui::TreeNode("Entry", "Effect %d: %s", index_ + 1, typeNames_[static_cast<int>(entry_.type)]))
        {
            ImGui::Checkbox("Enabled", &entry_.enabled);
            int type_ = static_cast<int>(entry_.type);
            if (ImGui::Combo("Type", &type_, typeNames_, IM_ARRAYSIZE(typeNames_)))
            {
                entry_.type = static_cast<IMPACT_EFFECT_ENTRY_TYPE>(type_);
                entry_.assetPath.clear();
            }
            if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::DECAL)
            {
                AssetSlotGui::Draw("Texture", AssetSlotType::TEXTURE, entry_.assetPath);
                ImGui::DragFloat3("Scale", &entry_.scale.x, 0.01f, 0.001f, 100.0f);
                ImGui::ColorEdit4("Color", &entry_.color.x);
                ImGui::DragFloat("Fade Out", &entry_.fadeOutDuration, 0.05f, 0.0f, entry_.lifeTime);
            }
            else
            {
                AssetSlotGui::Draw("Effect JSON", AssetSlotType::JSON, entry_.assetPath);
                if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::PARTICLE)
                {
                    ImGui::InputText("Emitter Name", &entry_.emitterName);
                }
            }
            ImGui::DragFloat("Life Time", &entry_.lifeTime, 0.05f, 0.01f, 120.0f);
            ImGui::DragFloat("Normal Offset", &entry_.normalOffset, 0.001f, -10.0f, 10.0f);
            if (ImGui::Button("Remove")) removeIndex_ = index_;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeIndex_ >= 0) asset.entries.erase(asset.entries.begin() + removeIndex_);
    if (ImGui::Button("Add Particle")) asset.entries.push_back({});
    ImGui::SameLine();
    if (ImGui::Button("Add Mesh VFX"))
    {
        ImpactEffectEntry entry_{};
        entry_.type = IMPACT_EFFECT_ENTRY_TYPE::MESH_VFX;
        asset.entries.push_back(entry_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Decal"))
    {
        ImpactEffectEntry entry_{};
        entry_.type = IMPACT_EFFECT_ENTRY_TYPE::DECAL;
        entry_.lifeTime = 5.0f;
        entry_.fadeOutDuration = 1.0f;
        entry_.normalOffset = 0.01f;
        asset.entries.push_back(entry_);
    }
}

void ImpactEffectComponent::Serialize(nlohmann::json& json_) const
{
    json_["ComponentName"] = componentName;
    json_["TargetLayerMask"] = targetLayerMask;
    json_["CoolDown"] = coolDown;
    json_["SpawnOnlyOnce"] = spawnOnlyOnce;
    json_["ListenToTriggers"] = listenToTriggers;
    json_["AssetPath"] = assetPath;
    json_["Entries"] = asset.entries;
}

void ImpactEffectComponent::Deserialize(const nlohmann::json& json_)
{
    targetLayerMask = json_.value("TargetLayerMask", 0xFFFFFFFFu);
    coolDown = (std::max)(json_.value("CoolDown", 0.1f), 0.0f);
    spawnOnlyOnce = json_.value("SpawnOnlyOnce", true);
    listenToTriggers = json_.value("ListenToTriggers", false);
    assetPath = json_.value("AssetPath", std::string{});
    asset.entries = json_.value("Entries", std::vector<ImpactEffectEntry>{});
    hasSpawned = false;
    coolDownTimer = 0.0f;
}

REGISTER_COMPONENT(ImpactEffectComponent, "ImpactEffectComponent")
