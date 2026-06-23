#include "WeaponTrailComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    bool ContainsCaseInsensitive(const std::string& text_, const std::string& searchText_)
    {
        if (searchText_.empty()) return true;

        auto toLower_ = [](unsigned char character_)
        {
            return static_cast<char>(std::tolower(character_));
        };
        std::string lowerText_(text_.size(), '\0');
        std::string lowerSearchText_(searchText_.size(), '\0');
        std::transform(text_.begin(), text_.end(), lowerText_.begin(), toLower_);
        std::transform(searchText_.begin(), searchText_.end(), lowerSearchText_.begin(), toLower_);
        return lowerText_.find(lowerSearchText_) != std::string::npos;
    }

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
    targetStartNodeName = nodeName_;
    InvalidateTarget();
}

void WeaponTrailComponent::SetTargetNodes(
    const std::string& startNodeName_,
    const std::string& endNodeName_)
{
    targetStartNodeName = startNodeName_;
    targetEndNodeName = endNodeName_;
    targetModel.reset();
    targetStartNodeIndex = -1;
    targetEndNodeIndex = -1;
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
    const std::shared_ptr<GltfModelCommponennt>& model_,
    const std::string& nodeName_,
    int& nodeIndex_)
{
    if (nodeName_.empty())
    {
        nodeIndex_ = -1;
        return false;
    }
    if (!model_) return false;

    const auto& nodes_ = model_->GetNodes();
    const bool validIndex_ = nodeIndex_ >= 0 &&
        nodeIndex_ < static_cast<int>(nodes_.size()) &&
        nodes_[nodeIndex_].name == nodeName_;
    if (!validIndex_) nodeIndex_ = model_->GetNodeIndex(nodeName_.c_str());
    return nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(nodes_.size());
}

bool WeaponTrailComponent::GetBladeSegment(
    DirectX::XMFLOAT3& rootPosition_,
    DirectX::XMFLOAT3& tipPosition_)
{
    const std::shared_ptr<GameObject> targetObject_ = ResolveTargetObject();
    if (!targetObject_ || !targetObject_->transform) return false;

    const std::shared_ptr<GltfModelCommponennt> model_ = ResolveTargetModel(targetObject_);
    const bool hasStartNode_ = ResolveTargetNodeIndex(
        model_, targetStartNodeName, targetStartNodeIndex);
    const bool hasEndNode_ = ResolveTargetNodeIndex(
        model_, targetEndNodeName, targetEndNodeIndex);

    // Explicit Start/End nodes describe the complete blade segment and take priority
    // over the procedural length and rotation settings.
    if (hasStartNode_ && hasEndNode_)
    {
        const DirectX::XMFLOAT3 nodeOrigin_ = { 0.0f, 0.0f, 0.0f };
        rootPosition_ = TransformPosition(
            nodeOrigin_,
            DirectX::XMLoadFloat4x4(&model_->GetNodes()[targetStartNodeIndex].worldTransform));
        tipPosition_ = TransformPosition(
            nodeOrigin_,
            DirectX::XMLoadFloat4x4(&model_->GetNodes()[targetEndNodeIndex].worldTransform));
        return true;
    }

    DirectX::XMMATRIX referenceTransform_ = targetObject_->transform->GetWorldMatrix();
    if (hasStartNode_)
    {
        referenceTransform_ = DirectX::XMLoadFloat4x4(
            &model_->GetNodes()[targetStartNodeIndex].worldTransform);
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
    targetStartNodeIndex = -1;
    targetEndNodeIndex = -1;
}

void WeaponTrailComponent::Gui()
{
    TrailMeshComponent::Gui();
    ImGui::SeparatorText("Weapon Trail");

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const char* objectPreview_ = targetGameObjectName.empty()
        ? "(Owner)"
        : targetGameObjectName.c_str();
    if (ImGui::BeginCombo("Target GameObject", objectPreview_))
    {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##ObjectSearch", "Search GameObject...", &targetObjectSearchText);
        if (ImGui::Selectable("(Owner)", targetGameObjectName.empty()))
        {
            targetGameObjectName.clear();
            InvalidateTarget();
        }
        if (owner_ && owner_->manager)
        {
            for (const std::shared_ptr<GameObject>& object_ : owner_->GetManager().GetObjects())
            {
                if (!object_ || !ContainsCaseInsensitive(object_->GetName(), targetObjectSearchText)) continue;
                const bool selected_ = object_->GetName() == targetGameObjectName;
                if (ImGui::Selectable(object_->GetName().c_str(), selected_))
                {
                    targetGameObjectName = object_->GetName();
                    InvalidateTarget();
                }
            }
        }
        ImGui::EndCombo();
    }

    const std::shared_ptr<GameObject> targetObject_ = ResolveTargetObject();
    const std::shared_ptr<GltfModelCommponennt> model_ = ResolveTargetModel(targetObject_);
    auto drawNodeCombo_ = [this, &model_](
        const char* label_,
        std::string& nodeName_,
        std::string& searchText_,
        int& nodeIndex_)
    {
        const char* preview_ = nodeName_.empty() ? "(None)" : nodeName_.c_str();
        if (!ImGui::BeginCombo(label_, preview_)) return;

        ImGui::PushID(label_);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##NodeSearch", "Search Node...", &searchText_);
        if (ImGui::Selectable("(None)", nodeName_.empty()))
        {
            nodeName_.clear();
            nodeIndex_ = -1;
        }
        if (model_)
        {
            const auto& nodes_ = model_->GetNodes();
            for (int index_ = 0; index_ < static_cast<int>(nodes_.size()); ++index_)
            {
                const std::string& candidateName_ = nodes_[index_].name;
                if (!ContainsCaseInsensitive(candidateName_, searchText_)) continue;
                const bool selected_ = candidateName_ == nodeName_;
                if (ImGui::Selectable(candidateName_.c_str(), selected_))
                {
                    nodeName_ = candidateName_;
                    nodeIndex_ = index_;
                }
            }
        }
        else
        {
            ImGui::TextDisabled("Target has no GLTF model.");
        }
        ImGui::PopID();
        ImGui::EndCombo();
    };
    drawNodeCombo_(
        "Start Node", targetStartNodeName, targetStartNodeSearchText, targetStartNodeIndex);
    drawNodeCombo_(
        "End Node", targetEndNodeName, targetEndNodeSearchText, targetEndNodeIndex);

    ImGui::DragFloat3("Blade Start Offset", &rootOffset.x, 0.01f);
    ImGui::DragFloat("Blade Length", &bladeLength, 0.01f, 0.0f, 10000.0f);
    ImGui::DragFloat3("Blade Rotation Offset", &rotationOffsetDegrees.x, 0.25f);
    ImGui::Checkbox("Show Blade Capsule", &showBladeGuide);
    if (showBladeGuide)
    {
        ImGui::DragFloat("Blade Capsule Radius", &bladeGuideRadius, 0.005f, 0.001f, 10.0f);
    }
    ImGui::TextDisabled("Rotation is in degrees. The base blade axis is local +Z.");
    ImGui::TextDisabled("Valid Start + End Nodes override offset, length, and rotation.");
    ImGui::TextDisabled("Without both Nodes, Start + Length + Rotation is used.");
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
    json_["WeaponTargetNodeName"] = targetStartNodeName;
    json_["WeaponTargetStartNodeName"] = targetStartNodeName;
    json_["WeaponTargetEndNodeName"] = targetEndNodeName;
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
    targetStartNodeName = json_.value(
        "WeaponTargetStartNodeName",
        json_.value("WeaponTargetNodeName", std::string{}));
    targetEndNodeName = json_.value("WeaponTargetEndNodeName", std::string{});
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
