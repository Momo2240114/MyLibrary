#include "WeaponTrailComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    DirectX::XMFLOAT3 TransformPosition(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMMATRIX& transform_)
    {
        DirectX::XMFLOAT3 result_{};
        DirectX::XMStoreFloat3(
            &result_,
            DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&position_), transform_));
        return result_;
    }

    void ReadPosition(
        const nlohmann::json& json_,
        const char* key_,
        DirectX::XMFLOAT3& position_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 3) return;
        position_ =
        {
            json_[key_][0].get<float>(),
            json_[key_][1].get<float>(),
            json_[key_][2].get<float>(),
        };
    }

    DirectX::XMFLOAT3 CalculateRotationOffset(
        const DirectX::XMFLOAT3& startOffset_,
        const DirectX::XMFLOAT3& tipOffset_)
    {
        const float deltaX_ = tipOffset_.x - startOffset_.x;
        const float deltaY_ = tipOffset_.y - startOffset_.y;
        const float deltaZ_ = tipOffset_.z - startOffset_.z;
        const float length_ = std::sqrt(
            deltaX_ * deltaX_ + deltaY_ * deltaY_ + deltaZ_ * deltaZ_);
        if (length_ <= 0.00001f) return { 0.0f, 0.0f, 0.0f };

        return
        {
            DirectX::XMConvertToDegrees(-std::asin((std::clamp)(deltaY_ / length_, -1.0f, 1.0f))),
            DirectX::XMConvertToDegrees(std::atan2(deltaX_, deltaZ_)),
            0.0f
        };
    }

    DirectX::XMMATRIX CreateSegmentTransform(
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_)
    {
        using namespace DirectX;
        const XMVECTOR start_ = XMLoadFloat3(&startPosition_);
        const XMVECTOR end_ = XMLoadFloat3(&endPosition_);
        const XMVECTOR direction_ = XMVector3Normalize(XMVectorSubtract(end_, start_));
        XMVECTOR referenceAxis_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        if (std::abs(XMVectorGetX(XMVector3Dot(direction_, referenceAxis_))) > 0.99f)
        {
            referenceAxis_ = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }

        const XMVECTOR xAxis_ = XMVector3Normalize(XMVector3Cross(direction_, referenceAxis_));
        const XMVECTOR zAxis_ = XMVector3Normalize(XMVector3Cross(xAxis_, direction_));
        const XMVECTOR center_ = XMVectorScale(XMVectorAdd(start_, end_), 0.5f);
        XMMATRIX transform_ = XMMatrixIdentity();
        transform_.r[0] = XMVectorSetW(xAxis_, 0.0f);
        transform_.r[1] = XMVectorSetW(direction_, 0.0f);
        transform_.r[2] = XMVectorSetW(zAxis_, 0.0f);
        transform_.r[3] = XMVectorSetW(center_, 1.0f);
        return transform_;
    }
}

WeaponTrailComponent::WeaponTrailComponent()
{
    SetEmitting(false);
}

void WeaponTrailComponent::LateUpdate(float deltaTime_)
{
    DirectX::XMFLOAT3 rootPosition_{};
    DirectX::XMFLOAT3 tipPosition_{};
    const bool hasBladeSegment_ = GetBladeSegment(rootPosition_, tipPosition_);
    if (IsEmitting() && hasBladeSegment_)
    {
        AddSample(rootPosition_, tipPosition_);
    }
    if (showBladeGuide && hasBladeSegment_)
    {
        DrawBladeGuide(rootPosition_, tipPosition_);
    }

    // Upload after sampling so the new segment is visible in this frame.
    TrailMeshComponent::LateUpdate(deltaTime_);
}

void WeaponTrailComponent::SetTarget(
    const std::string& gameObjectName_,
    const std::string& nodeName_)
{
    targetGameObjectName = gameObjectName_;
    targetNodeName = nodeName_;
    InvalidateTarget();
}

void WeaponTrailComponent::SetBladeOffsets(
    const DirectX::XMFLOAT3& rootOffset_,
    const DirectX::XMFLOAT3& tipOffset_)
{
    rootOffset = rootOffset_;
    const float deltaX_ = tipOffset_.x - rootOffset_.x;
    const float deltaY_ = tipOffset_.y - rootOffset_.y;
    const float deltaZ_ = tipOffset_.z - rootOffset_.z;
    bladeLength = std::sqrt(
        deltaX_ * deltaX_ + deltaY_ * deltaY_ + deltaZ_ * deltaZ_);
    rotationOffsetDegrees = CalculateRotationOffset(rootOffset_, tipOffset_);
}

void WeaponTrailComponent::SetBladeShape(
    const DirectX::XMFLOAT3& startOffset_,
    float length_,
    const DirectX::XMFLOAT3& rotationOffsetDegrees_)
{
    rootOffset = startOffset_;
    bladeLength = (std::max)(length_, 0.0f);
    rotationOffsetDegrees = rotationOffsetDegrees_;
}

void WeaponTrailComponent::BeginTrail(bool clearExisting_)
{
    if (clearExisting_) Clear();
    SetEmitting(true);
}

void WeaponTrailComponent::EndTrail(bool clearImmediately_)
{
    SetEmitting(false);
    if (clearImmediately_) Clear();
}

std::shared_ptr<GameObject> WeaponTrailComponent::ResolveTargetObject()
{
    if (auto targetObject_ = targetObject.lock())
    {
        if (targetGameObjectName.empty() || targetObject_->GetName() == targetGameObjectName)
        {
            return targetObject_;
        }
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    if (targetGameObjectName.empty() || owner_->GetName() == targetGameObjectName)
    {
        targetObject = owner_;
        if (targetGameObjectName.empty()) targetGameObjectName = owner_->GetName();
        return owner_;
    }

    if (owner_->manager)
    {
        if (auto foundObject_ = owner_->GetManager().Find(targetGameObjectName))
        {
            targetObject = foundObject_;
            return foundObject_;
        }
    }
    return nullptr;
}

std::shared_ptr<GltfModelCommponennt> WeaponTrailComponent::ResolveTargetModel(
    const std::shared_ptr<GameObject>& targetObject_)
{
    if (auto model_ = targetModel.lock()) return model_;
    if (!targetObject_) return nullptr;

    if (auto model_ = targetObject_->GetComponent<GltfModelCommponennt>())
    {
        targetModel = model_;
        return model_;
    }

    auto parent_ = targetObject_->transform ? targetObject_->transform->parent.lock() : nullptr;
    while (parent_)
    {
        const std::shared_ptr<GameObject> parentObject_ = parent_->gameObject.lock();
        if (parentObject_)
        {
            if (auto model_ = parentObject_->GetComponent<GltfModelCommponennt>())
            {
                targetModel = model_;
                return model_;
            }
        }
        parent_ = parent_->parent.lock();
    }
    return nullptr;
}

bool WeaponTrailComponent::ResolveTargetNodeIndex(
    const std::shared_ptr<GltfModelCommponennt>& model_)
{
    if (targetNodeName.empty())
    {
        targetNodeIndex = -1;
        return true;
    }
    if (!model_) return false;

    const auto& nodes_ = model_->GetNodes();
    const bool validIndex_ = targetNodeIndex >= 0 &&
        targetNodeIndex < static_cast<int>(nodes_.size()) &&
        nodes_[targetNodeIndex].name == targetNodeName;
    if (!validIndex_) targetNodeIndex = model_->GetNodeIndex(targetNodeName.c_str());
    return targetNodeIndex >= 0 && targetNodeIndex < static_cast<int>(nodes_.size());
}

bool WeaponTrailComponent::GetBladeSegment(
    DirectX::XMFLOAT3& rootPosition_,
    DirectX::XMFLOAT3& tipPosition_)
{
    const std::shared_ptr<GameObject> targetObject_ = ResolveTargetObject();
    if (!targetObject_ || !targetObject_->transform) return false;

    DirectX::XMMATRIX referenceTransform_ = targetObject_->transform->GetWorldMatrix();
    if (!targetNodeName.empty())
    {
        const std::shared_ptr<GltfModelCommponennt> model_ = ResolveTargetModel(targetObject_);
        if (!ResolveTargetNodeIndex(model_)) return false;
        referenceTransform_ = DirectX::XMLoadFloat4x4(
            &model_->GetNodes()[targetNodeIndex].worldTransform);
    }

    const DirectX::XMMATRIX rotationOffset_ = DirectX::XMMatrixRotationRollPitchYaw(
        DirectX::XMConvertToRadians(rotationOffsetDegrees.x),
        DirectX::XMConvertToRadians(rotationOffsetDegrees.y),
        DirectX::XMConvertToRadians(rotationOffsetDegrees.z));
    DirectX::XMFLOAT3 bladeDirection_{};
    DirectX::XMStoreFloat3(
        &bladeDirection_,
        DirectX::XMVector3TransformNormal(
            DirectX::XMVectorSet(0.0f, 0.0f, bladeLength, 0.0f),
            rotationOffset_));
    const DirectX::XMFLOAT3 tipOffset_
    {
        rootOffset.x + bladeDirection_.x,
        rootOffset.y + bladeDirection_.y,
        rootOffset.z + bladeDirection_.z
    };

    rootPosition_ = TransformPosition(rootOffset, referenceTransform_);
    tipPosition_ = TransformPosition(tipOffset_, referenceTransform_);
    return true;
}

void WeaponTrailComponent::DrawBladeGuide(
    const DirectX::XMFLOAT3& startPosition_,
    const DirectX::XMFLOAT3& endPosition_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;

    const std::shared_ptr<GameObject> selectedObject_ = owner_->GetManager().GetSelectedObject();
    const std::shared_ptr<GameObject> targetObject_ = targetObject.lock();
    if (selectedObject_ != owner_ && selectedObject_ != targetObject_) return;

    const float deltaX_ = endPosition_.x - startPosition_.x;
    const float deltaY_ = endPosition_.y - startPosition_.y;
    const float deltaZ_ = endPosition_.z - startPosition_.z;
    const float length_ = std::sqrt(
        deltaX_ * deltaX_ + deltaY_ * deltaY_ + deltaZ_ * deltaZ_);
    if (length_ <= 0.00001f) return;

    DirectX::XMFLOAT4X4 transform_{};
    DirectX::XMStoreFloat4x4(
        &transform_,
        CreateSegmentTransform(startPosition_, endPosition_));
    GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawCapsule(
        transform_,
        (std::max)(bladeGuideRadius, 0.001f),
        length_,
        IsEmitting()
            ? DirectX::XMFLOAT4(1.0f, 0.5f, 0.1f, 1.0f)
            : DirectX::XMFLOAT4(0.1f, 0.8f, 1.0f, 1.0f));
}

void WeaponTrailComponent::InvalidateTarget()
{
    targetObject.reset();
    targetModel.reset();
    targetNodeIndex = -1;
}

void WeaponTrailComponent::Gui()
{
    TrailMeshComponent::Gui();
    ImGui::SeparatorText("Weapon Trail");
    if (ImGui::InputText("Target GameObject", &targetGameObjectName)) InvalidateTarget();
    if (ImGui::InputText("Target Node", &targetNodeName))
    {
        targetModel.reset();
        targetNodeIndex = -1;
    }
    ImGui::DragFloat3("Blade Start Offset", &rootOffset.x, 0.01f);
    ImGui::DragFloat("Blade Length", &bladeLength, 0.01f, 0.0f, 10000.0f);
    ImGui::DragFloat3("Blade Rotation Offset", &rotationOffsetDegrees.x, 0.25f);
    ImGui::Checkbox("Show Blade Capsule", &showBladeGuide);
    if (showBladeGuide)
    {
        ImGui::DragFloat("Blade Capsule Radius", &bladeGuideRadius, 0.005f, 0.001f, 10.0f);
    }
    ImGui::TextDisabled("Rotation is in degrees. The base blade axis is local +Z.");
    ImGui::TextDisabled("Empty Target Node uses the target GameObject transform.");
    if (ImGui::Button("Begin Weapon Trail")) BeginTrail();
    ImGui::SameLine();
    if (ImGui::Button("End Weapon Trail")) EndTrail();
    ImGui::SameLine();
    if (ImGui::Button("Resolve Weapon Again")) InvalidateTarget();
}

void WeaponTrailComponent::Serialize(nlohmann::json& json_) const
{
    TrailMeshComponent::Serialize(json_);
    json_["WeaponTargetGameObjectName"] = targetGameObjectName;
    json_["WeaponTargetNodeName"] = targetNodeName;
    json_["WeaponRootOffset"] = { rootOffset.x, rootOffset.y, rootOffset.z };
    json_["WeaponBladeLength"] = bladeLength;
    json_["WeaponRotationOffsetDegrees"] =
    {
        rotationOffsetDegrees.x,
        rotationOffsetDegrees.y,
        rotationOffsetDegrees.z
    };
    json_["WeaponShowBladeGuide"] = showBladeGuide;
    json_["WeaponBladeGuideRadius"] = bladeGuideRadius;
}

void WeaponTrailComponent::Deserialize(const nlohmann::json& json_)
{
    TrailMeshComponent::Deserialize(json_);
    if (!json_.contains("TrailEmitting")) SetEmitting(false);
    targetGameObjectName = json_.value("WeaponTargetGameObjectName", std::string{});
    targetNodeName = json_.value("WeaponTargetNodeName", std::string{});
    ReadPosition(json_, "WeaponRootOffset", rootOffset);
    if (json_.contains("WeaponBladeLength"))
    {
        bladeLength = (std::max)(json_.value("WeaponBladeLength", 1.0f), 0.0f);
        ReadPosition(json_, "WeaponRotationOffsetDegrees", rotationOffsetDegrees);
    }
    else
    {
        DirectX::XMFLOAT3 legacyTipOffset_ = { 0.0f, 0.0f, 1.0f };
        ReadPosition(json_, "WeaponTipOffset", legacyTipOffset_);
        SetBladeOffsets(rootOffset, legacyTipOffset_);
    }
    showBladeGuide = json_.value("WeaponShowBladeGuide", true);
    bladeGuideRadius = (std::max)(json_.value("WeaponBladeGuideRadius", 0.03f), 0.001f);
    InvalidateTarget();
}

REGISTER_COMPONENT(WeaponTrailComponent, "WeaponTrailComponent")
