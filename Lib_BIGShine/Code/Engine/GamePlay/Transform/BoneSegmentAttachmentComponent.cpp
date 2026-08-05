#include "PrecompiledHeader.h"

#include "BoneSegmentAttachmentComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Physics/CapsuleCollider.h"

#include <algorithm>
#include <cmath>
#include <imgui_stdlib.h>

using namespace DirectX;

namespace
{
XMFLOAT3 TransformOffset(const XMFLOAT3& offset_, const XMFLOAT4X4& transform_)
{
    // NodeのWorldTransformにローカルオフセットを乗せ、実際の端点位置へ変換する。
    XMFLOAT3 position_;
    XMStoreFloat3(
        &position_,
        XMVector3TransformCoord(XMLoadFloat3(&offset_), XMLoadFloat4x4(&transform_)));
    return position_;
}

XMMATRIX CreateSegmentTransform(
    const XMFLOAT3& startPosition_,
    const XMFLOAT3& endPosition_,
    float thickness_)
{
    // start-endをY軸方向とする細長いTransformを作り、MeshVFXやCapsuleへ流用する。
    const XMVECTOR start_ = XMLoadFloat3(&startPosition_);
    const XMVECTOR end_ = XMLoadFloat3(&endPosition_);
    const XMVECTOR difference_ = XMVectorSubtract(end_, start_);
    const float length_ = XMVectorGetX(XMVector3Length(difference_));
    const XMVECTOR yAxis_ = XMVector3Normalize(difference_);

    XMVECTOR referenceAxis_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    if (std::abs(XMVectorGetX(XMVector3Dot(yAxis_, referenceAxis_))) > 0.99f)
    {
        // 線分がZ軸とほぼ平行な場合、外積が潰れないよう参照軸をXへ変える。
        referenceAxis_ = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const XMVECTOR xAxis_ = XMVector3Normalize(XMVector3Cross(yAxis_, referenceAxis_));
    const XMVECTOR zAxis_ = XMVector3Normalize(XMVector3Cross(xAxis_, yAxis_));
    const XMVECTOR center_ = XMVectorScale(XMVectorAdd(start_, end_), 0.5f);

    XMMATRIX rotationTranslation_ = XMMatrixIdentity();
    rotationTranslation_.r[0] = XMVectorSetW(xAxis_, 0.0f);
    rotationTranslation_.r[1] = XMVectorSetW(yAxis_, 0.0f);
    rotationTranslation_.r[2] = XMVectorSetW(zAxis_, 0.0f);
    rotationTranslation_.r[3] = XMVectorSetW(center_, 1.0f);
    return XMMatrixScaling(thickness_, length_, thickness_) * rotationTranslation_;
}
}

void BoneSegmentAttachmentComponent::Update(float deltaTime_)
{
    SyncTargets();
}

void BoneSegmentAttachmentComponent::LateUpdate(float deltaTime_)
{
    SyncTargets();
}

std::shared_ptr<GltfModelCommponennt> BoneSegmentAttachmentComponent::ResolveTargetModel()
{
    // 既に解決済みならキャッシュを使う。
    if (auto model_ = targetModel.lock()) return model_;

    auto owner_ = GetGameObject();
    if (!owner_) return nullptr;

    if (!targetGameObjectName.empty() && owner_->manager)
    {
        // 明示指定されたGameObject名を優先してModelを探す。
        if (auto targetObject_ = owner_->GetManager().Find(targetGameObjectName))
        {
            if (auto model_ = targetObject_->GetComponent<GltfModelCommponennt>())
            {
                targetModel = model_;
                return model_;
            }
        }
    }

    if (auto model_ = owner_->GetComponent<GltfModelCommponennt>())
    {
        // 指定が無ければ所有者自身のModelを使う。
        targetModel = model_;
        targetGameObjectName = owner_->GetName();
        return model_;
    }

    auto parent_ = owner_->transform->parent.lock();
    while (parent_)
    {
        // エフェクトやColliderが子Objectに付くケースに備え、親階層も探索する。
        auto parentObject_ = parent_->gameObject.lock();
        if (parentObject_)
        {
            if (auto model_ = parentObject_->GetComponent<GltfModelCommponennt>())
            {
                targetModel = model_;
                targetGameObjectName = parentObject_->GetName();
                return model_;
            }
        }
        parent_ = parent_->parent.lock();
    }
    return nullptr;
}

bool BoneSegmentAttachmentComponent::ResolveNodeIndices(
    const std::shared_ptr<GltfModelCommponennt>& model_)
{
    if (!model_ || startNodeName.empty() || endNodeName.empty()) return false;
    model_->UpdateLoadState();
    if (!model_->IsModelReady()) return false;

    const auto& nodes_ = model_->GetNodes();
    const bool validStart_ = startNodeIndex >= 0 &&
        startNodeIndex < static_cast<int>(nodes_.size()) &&
        nodes_[startNodeIndex].name == startNodeName;
    const bool validEnd_ = endNodeIndex >= 0 &&
        endNodeIndex < static_cast<int>(nodes_.size()) &&
        nodes_[endNodeIndex].name == endNodeName;

    if (!validStart_) startNodeIndex = model_->GetNodeIndex(startNodeName.c_str());
    if (!validEnd_) endNodeIndex = model_->GetNodeIndex(endNodeName.c_str());
    // Node名が変わっていなければキャッシュを使い、変わった時だけ再検索する。
    return startNodeIndex >= 0 && endNodeIndex >= 0;
}

bool BoneSegmentAttachmentComponent::IsLoadReady() const
{
    auto model_ = const_cast<BoneSegmentAttachmentComponent*>(this)->ResolveTargetModel();
    if (!model_) return true;

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

bool BoneSegmentAttachmentComponent::GetSegment(
    XMFLOAT3& startPosition_,
    XMFLOAT3& endPosition_) const
{
    auto model_ = targetModel.lock();
    if (!model_) return false;
    model_->UpdateLoadState();
    if (!model_->IsModelReady()) return false;

    const auto& nodes_ = model_->GetNodes();
    if (startNodeIndex < 0 || endNodeIndex < 0 ||
        startNodeIndex >= static_cast<int>(nodes_.size()) ||
        endNodeIndex >= static_cast<int>(nodes_.size()))
    {
        return false;
    }

    startPosition_ = TransformOffset(startOffset, nodes_[startNodeIndex].worldTransform);
    endPosition_ = TransformOffset(endOffset, nodes_[endNodeIndex].worldTransform);
    return true;
}

void BoneSegmentAttachmentComponent::SyncTargets()
{
    // Node間線分を求め、Transform/Particle/Capsuleへ同じ結果を同期する。
    auto owner_ = GetGameObject();
    auto model_ = ResolveTargetModel();
    if (!owner_ || !ResolveNodeIndices(model_)) return;

    XMFLOAT3 startPosition_;
    XMFLOAT3 endPosition_;
    if (!GetSegment(startPosition_, endPosition_)) return;

    const XMVECTOR difference_ = XMVectorSubtract(
        XMLoadFloat3(&endPosition_), XMLoadFloat3(&startPosition_));
    const float length_ = XMVectorGetX(XMVector3Length(difference_));
    if (length_ <= 0.00001f) return;

    const XMMATRIX segmentTransform_ =
        CreateSegmentTransform(startPosition_, endPosition_, (std::max)(meshVFXThickness, 0.0001f));
    if (applyTransform || applyCapsule)
    {
        // 所有ObjectのTransformを線分中心・線分方向へ合わせる。
        owner_->transform->ApplyWorldMatrix(segmentTransform_);
    }

    if (applyParticleLine)
    {
        // ParticleのLine発生形状を、現在のBone Segmentへ追従させる。
        if (auto particle_ = owner_->GetComponent<ParticleComponent>())
        {
            particle_->SetEmitterLine(particleEmitterName, startPosition_, endPosition_);
        }
    }

    if (applyCapsule)
    {
        // CapsuleColliderはObject Transform側で長さを表し、Collider側は単位長の半分を使う。
        if (auto capsule_ = owner_->GetComponent<CapsuleCollider>())
        {
            capsule_->radius = capsuleRadius / (std::max)(meshVFXThickness, 0.0001f);
            capsule_->halfHeight = 0.5f;
        }
    }
}

void BoneSegmentAttachmentComponent::Gui()
{
    if (ImGui::InputText("Target GameObject", &targetGameObjectName))
    {
        targetModel.reset();
        startNodeIndex = -1;
        endNodeIndex = -1;
    }
    if (ImGui::InputText("Start Node", &startNodeName)) startNodeIndex = -1;
    if (ImGui::InputText("End Node", &endNodeName)) endNodeIndex = -1;
    ImGui::DragFloat3("Start Offset", &startOffset.x, 0.01f);
    ImGui::DragFloat3("End Offset", &endOffset.x, 0.01f);

    ImGui::Checkbox("Apply Segment Transform / Mesh VFX", &applyTransform);
    ImGui::DragFloat("Mesh VFX Thickness", &meshVFXThickness, 0.01f, 0.0001f, 100.0f);

    ImGui::Checkbox("Apply Particle Line", &applyParticleLine);
    ImGui::InputText("Particle Emitter", &particleEmitterName);
    ImGui::TextDisabled("Leave emitter empty to apply to every emitter.");

    ImGui::Checkbox("Apply Capsule", &applyCapsule);
    ImGui::DragFloat("Capsule Radius", &capsuleRadius, 0.01f, 0.0f, 100.0f);

    if (ImGui::Button("Resolve Again"))
    {
        targetModel.reset();
        startNodeIndex = -1;
        endNodeIndex = -1;
    }
}

void BoneSegmentAttachmentComponent::Serialize(nlohmann::json& json_) const
{
    json_["TargetGameObjectName"] = targetGameObjectName;
    json_["StartNodeName"] = startNodeName;
    json_["EndNodeName"] = endNodeName;
    json_["ParticleEmitterName"] = particleEmitterName;
    json_["StartOffset"] = { startOffset.x, startOffset.y, startOffset.z };
    json_["EndOffset"] = { endOffset.x, endOffset.y, endOffset.z };
    json_["MeshVFXThickness"] = meshVFXThickness;
    json_["CapsuleRadius"] = capsuleRadius;
    json_["ApplyTransform"] = applyTransform;
    json_["ApplyParticleLine"] = applyParticleLine;
    json_["ApplyCapsule"] = applyCapsule;
}

void BoneSegmentAttachmentComponent::Deserialize(const nlohmann::json& json_)
{
    targetGameObjectName = json_.value("TargetGameObjectName", std::string{});
    startNodeName = json_.value("StartNodeName", std::string{});
    endNodeName = json_.value("EndNodeName", std::string{});
    particleEmitterName = json_.value("ParticleEmitterName", std::string{});
    meshVFXThickness = json_.value("MeshVFXThickness", 1.0f);
    capsuleRadius = json_.value("CapsuleRadius", 0.1f);
    applyTransform = json_.value("ApplyTransform", true);
    applyParticleLine = json_.value("ApplyParticleLine", true);
    applyCapsule = json_.value("ApplyCapsule", true);

    if (json_.contains("StartOffset") && json_["StartOffset"].size() >= 3)
    {
        startOffset = {
            json_["StartOffset"][0].get<float>(),
            json_["StartOffset"][1].get<float>(),
            json_["StartOffset"][2].get<float>() };
    }
    if (json_.contains("EndOffset") && json_["EndOffset"].size() >= 3)
    {
        endOffset = {
            json_["EndOffset"][0].get<float>(),
            json_["EndOffset"][1].get<float>(),
            json_["EndOffset"][2].get<float>() };
    }

    targetModel.reset();
    // Deserialize後はModel/Node参照を必ず解決し直す。
    startNodeIndex = -1;
    endNodeIndex = -1;
}
