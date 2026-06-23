#include "AfterimageComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

#include <algorithm>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <utility>

AfterimageComponent::AfterimageComponent()
{
    MeshVFXPart part_;
    part_.renderEnabled = true;
    part_.useMaterialBaseTexture = false;
    part_.useMaterialNormalTexture = false;
    part_.color = { 0.2f, 0.75f, 1.0f, 1.0f };
    part_.alpha = 1.0f;
    part_.useFresnel = true;
    part_.fresnelPower = 2.0f;
    part_.fresnelStrength = 0.8f;
    part_.blendMode = MeshVFXBlendMode::ADDITIVE;
    GetAsset().parts.push_back(part_);
}

void AfterimageComponent::Start()
{
    MeshVFXComponent::Start();
    RegisterActions();
}

void AfterimageComponent::Update(float deltaTime_)
{
    MeshVFXComponent::Update(deltaTime_);
}

void AfterimageComponent::LateUpdate(float deltaTime_)
{

    for (Snapshot& snapshot_ : snapshots)
    {
        snapshot_.age += deltaTime_;
    }
    snapshots.erase(
        std::remove_if(
            snapshots.begin(), snapshots.end(),
            [this](const Snapshot& snapshot_) { return snapshot_.age >= lifetime; }),
        snapshots.end());

    if (!isEmitting && pendingBurstCount <= 0) return;

    spawnTimer += deltaTime_;
    const float safeInterval_ = (std::max)(spawnInterval, 0.001f);
    while (spawnTimer >= safeInterval_ && (isEmitting || pendingBurstCount > 0))
    {
        spawnTimer -= safeInterval_;
        CaptureSnapshot();
        if (pendingBurstCount > 0)
        {
            --pendingBurstCount;
        }
    }
}

void AfterimageComponent::Gui()
{
    ImGui::SeparatorText("Afterimage");
    ImGui::DragFloat("Spawn Interval", &spawnInterval, 0.005f, 0.001f, 1.0f);
    ImGui::DragFloat("Lifetime", &lifetime, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Start Alpha", &startAlpha, 0.01f, 0.0f, 1.0f);
    ImGui::DragInt("Max Snapshots", &maxSnapshots, 1.0f, 1, 64);
    ImGui::DragInt("Burst Count", &burstCount, 1.0f, 1, 32);
    ImGui::Checkbox("Include Children", &includeChildren);
    ImGui::Text("Active: %zu", snapshots.size());

    if (ImGui::Button(isEmitting ? "Stop Emission" : "Start Emission"))
    {
        isEmitting ? StopEmission() : StartEmission();
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst")) Burst();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) Clear();

    MeshVFXComponent::Gui();
}

void AfterimageComponent::Serialize(nlohmann::json& json_) const
{
    MeshVFXComponent::Serialize(json_);
    json_["SpawnInterval"] = spawnInterval;
    json_["Lifetime"] = lifetime;
    json_["StartAlpha"] = startAlpha;
    json_["MaxSnapshots"] = maxSnapshots;
    json_["BurstCount"] = burstCount;
    json_["IncludeChildren"] = includeChildren;
}

void AfterimageComponent::Deserialize(const nlohmann::json& json_)
{
    MeshVFXComponent::Deserialize(json_);
    spawnInterval = (std::max)(json_.value("SpawnInterval", 0.06f), 0.001f);
    lifetime = (std::max)(json_.value("Lifetime", 0.4f), 0.01f);
    startAlpha = std::clamp(json_.value("StartAlpha", 0.8f), 0.0f, 1.0f);
    maxSnapshots = std::clamp(json_.value("MaxSnapshots", 8), 1, 64);
    burstCount = std::clamp(json_.value("BurstCount", 3), 1, 32);
    includeChildren = json_.value("IncludeChildren", true);
    Clear();
}

bool AfterimageComponent::GetModelPoseViews(std::vector<MeshVFXModelPoseView>& poses_) const
{
    poses_.clear();
    for (const Snapshot& snapshot_ : snapshots)
    {
        const float normalizedAge_ = std::clamp(snapshot_.age / lifetime, 0.0f, 1.0f);
        const float alpha_ = startAlpha * (1.0f - normalizedAge_);
        for (const MeshVFXModelPose& sourcePose_ : snapshot_.poses)
        {
            MeshVFXModelPoseView pose_;
            pose_.model = sourcePose_.model;
            pose_.meshTransforms = &sourcePose_.meshTransforms;
            pose_.time = snapshot_.age;
            pose_.alpha = alpha_;
            poses_.push_back(std::move(pose_));
        }
    }
    return !poses_.empty();
}

void AfterimageComponent::StartEmission()
{
    isEmitting = true;
    spawnTimer = spawnInterval;
}

void AfterimageComponent::StopEmission()
{
    isEmitting = false;
    pendingBurstCount = 0;
}

void AfterimageComponent::Burst()
{
    pendingBurstCount = burstCount;
    spawnTimer = spawnInterval;
}

void AfterimageComponent::Clear()
{
    snapshots.clear();
    pendingBurstCount = 0;
    spawnTimer = 0.0f;
}

void AfterimageComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    auto& registry_ = owner_->GetLocalRegistry();
    registry_.RegisterAction("VFX", "AfterimageStart", [this]() { StartEmission(); });
    registry_.RegisterAction("VFX", "AfterimageStop", [this]() { StopEmission(); });
    registry_.RegisterAction("VFX", "AfterimageBurst", [this]() { Burst(); });
    registry_.RegisterAction("VFX", "AfterimageClear", [this]() { Clear(); });
}

void AfterimageComponent::CaptureSnapshot()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    Snapshot snapshot_;
    CaptureObject(owner_, snapshot_);
    if (snapshot_.poses.empty()) return;

    snapshots.push_back(std::move(snapshot_));
    while (static_cast<int>(snapshots.size()) > maxSnapshots)
    {
        snapshots.erase(snapshots.begin());
    }
}

void AfterimageComponent::CaptureObject(
    const std::shared_ptr<GameObject>& object_,
    Snapshot& snapshot_)
{
    if (!object_ || !object_->transform) return;

    const std::shared_ptr<GltfModelCommponennt> model_ =
        object_->GetComponent<GltfModelCommponennt>();
    if (model_)
    {
        model_->UpdateTransform();
        MeshVFXModelPose pose_;
        pose_.model = model_;
        pose_.meshTransforms.resize(model_->GetMeshes().size());
        for (size_t meshIndex_ = 0; meshIndex_ < model_->GetMeshes().size(); ++meshIndex_)
        {
            const GltfModelCommponennt::Mesh& mesh_ = model_->GetMeshes()[meshIndex_];
            std::vector<DirectX::XMFLOAT4X4>& transforms_ = pose_.meshTransforms[meshIndex_];
            if (!mesh_.bones.empty())
            {
                const size_t boneCount_ = (std::min)(mesh_.bones.size(), static_cast<size_t>(256));
                transforms_.resize(boneCount_);
                for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
                {
                    const GltfModelCommponennt::Bone& bone_ = mesh_.bones[boneIndex_];
                    const DirectX::XMMATRIX offset_ =
                        DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
                    const DirectX::XMMATRIX nodeWorld_ = bone_.node
                        ? DirectX::XMLoadFloat4x4(&bone_.node->worldTransform)
                        : DirectX::XMMatrixIdentity();
                    DirectX::XMStoreFloat4x4(
                        &transforms_[boneIndex_], offset_ * nodeWorld_);
                }
            }
            else
            {
                transforms_.resize(1);
                const DirectX::XMMATRIX nodeWorld_ = mesh_.node
                    ? DirectX::XMLoadFloat4x4(&mesh_.node->worldTransform)
                    : DirectX::XMMatrixIdentity();
                DirectX::XMStoreFloat4x4(&transforms_[0], nodeWorld_);
            }
        }
        snapshot_.poses.push_back(std::move(pose_));
    }

    if (!includeChildren) return;
    for (const std::shared_ptr<Transform>& childTransform_ : object_->transform->children)
    {
        if (!childTransform_) continue;
        CaptureObject(childTransform_->gameObject.lock(), snapshot_);
    }
}
