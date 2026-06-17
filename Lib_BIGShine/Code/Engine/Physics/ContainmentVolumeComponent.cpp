#include "ContainmentVolumeComponent.h"

#include <algorithm>
#include <cmath>

#include "ColliderComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

namespace
{
DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
{
    return { a_.x - b_.x, a_.y - b_.y, a_.z - b_.z };
}

DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
{
    return { a_.x + b_.x, a_.y + b_.y, a_.z + b_.z };
}

DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3& value_, float scale_)
{
    return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
}

float Dot(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
{
    return a_.x * b_.x + a_.y * b_.y + a_.z * b_.z;
}

float LengthSquared(const DirectX::XMFLOAT3& value_)
{
    return Dot(value_, value_);
}

DirectX::XMFLOAT3 NormalizeOrZero(const DirectX::XMFLOAT3& value_)
{
    const float lengthSquared_ = LengthSquared(value_);
    if (lengthSquared_ <= 0.000001f)
    {
        return { 0.0f, 0.0f, 0.0f };
    }

    return Scale(value_, 1.0f / std::sqrt(lengthSquared_));
}

void RemoveOutwardVelocity(
    const std::shared_ptr<GameObject>& target_,
    const DirectX::XMFLOAT3& outwardNormal_)
{
    auto collider_ = target_ ? target_->GetComponent<ColliderComponent>() : nullptr;
    if (!collider_) return;

    const float outwardSpeed_ = Dot(collider_->velocity, outwardNormal_);
    if (outwardSpeed_ <= 0.0f) return;

    collider_->velocity = Subtract(collider_->velocity, Scale(outwardNormal_, outwardSpeed_));
}
}

void ContainmentVolumeComponent::ApplyContainment()
{
    auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;

    for (const auto& target_ : owner_->GetManager().GetObjects())
    {
        if (!target_ || target_.get() == owner_.get()) continue;
        if ((static_cast<uint32_t>(target_->layer) & targetMask) == 0) continue;

        ConstrainObject(target_);
    }
}

void ContainmentVolumeComponent::ConstrainObject(const std::shared_ptr<GameObject>& target_)
{
    auto owner_ = GetGameObject();
    if (!owner_ || !target_) return;

    const DirectX::XMFLOAT3 center_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 scale_ = owner_->transform->GetWorldScale();
    const float volumeRadius_ = (std::max)(radius * (std::max)(std::abs(scale_.x), std::abs(scale_.z)) - targetRadius, 0.001f);
    const float volumeHalfHeight_ = (std::max)(halfHeight * std::abs(scale_.y) - targetRadius, 0.001f);

    DirectX::XMFLOAT3 position_ = target_->transform->GetWorldPosition();
    DirectX::XMFLOAT3 corrected_ = position_;

    if (shape == CONTAINMENT_SHAPE::CYLINDER)
    {
        if (constrainHeight)
        {
            corrected_.y = std::clamp(position_.y, center_.y - volumeHalfHeight_, center_.y + volumeHalfHeight_);
        }

        const float dx_ = position_.x - center_.x;
        const float dz_ = position_.z - center_.z;
        const float horizontalLengthSquared_ = dx_ * dx_ + dz_ * dz_;
        if (horizontalLengthSquared_ > volumeRadius_ * volumeRadius_)
        {
            const float inverseLength_ = 1.0f / std::sqrt(horizontalLengthSquared_);
            corrected_.x = center_.x + dx_ * inverseLength_ * volumeRadius_;
            corrected_.z = center_.z + dz_ * inverseLength_ * volumeRadius_;
        }
    }
    else
    {
        const float segmentMinY_ = center_.y - volumeHalfHeight_;
        const float segmentMaxY_ = center_.y + volumeHalfHeight_;
        const float closestY_ = constrainHeight ? std::clamp(position_.y, segmentMinY_, segmentMaxY_) : position_.y;
        const DirectX::XMFLOAT3 closestOnSegment_{ center_.x, closestY_, center_.z };
        const DirectX::XMFLOAT3 fromAxis_ = Subtract(position_, closestOnSegment_);
        const float distanceSquared_ = LengthSquared(fromAxis_);

        if (distanceSquared_ > volumeRadius_ * volumeRadius_)
        {
            corrected_ = Add(closestOnSegment_, Scale(NormalizeOrZero(fromAxis_), volumeRadius_));
        }
    }

    const DirectX::XMFLOAT3 correction_ = Subtract(corrected_, position_);
    if (LengthSquared(correction_) <= 0.000001f)
    {
        return;
    }

    const DirectX::XMFLOAT3 outwardNormal_ = NormalizeOrZero(Subtract(position_, corrected_));
    DirectX::XMMATRIX world_ = target_->transform->GetWorldMatrix();
    world_.r[3] = DirectX::XMVectorSet(corrected_.x, corrected_.y, corrected_.z, 1.0f);
    target_->transform->ApplyWorldMatrix(world_);
    RemoveOutwardVelocity(target_, outwardNormal_);
}

void ContainmentVolumeComponent::Gui()
{
    const char* shapeNames_[] = { "Cylinder", "Capsule" };
    int shapeIndex_ = static_cast<int>(shape);
    if (ImGui::Combo("Shape", &shapeIndex_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
    {
        shape = static_cast<CONTAINMENT_SHAPE>(shapeIndex_);
    }

    ImGui::DragFloat("Radius", &radius, 0.05f, 0.01f, 1000.0f);
    ImGui::DragFloat("Half Height", &halfHeight, 0.05f, 0.01f, 1000.0f);
    ImGui::DragFloat("Target Radius", &targetRadius, 0.01f, 0.0f, 100.0f);
    ImGui::Checkbox("Constrain Height", &constrainHeight);
    ImGui::InputScalar("Target Mask", ImGuiDataType_U32, &targetMask);
}

void ContainmentVolumeComponent::Serialize(nlohmann::json& json_) const
{
    json_["Shape"] = static_cast<int>(shape);
    json_["TargetMask"] = targetMask;
    json_["Radius"] = radius;
    json_["HalfHeight"] = halfHeight;
    json_["TargetRadius"] = targetRadius;
    json_["ConstrainHeight"] = constrainHeight;
}

void ContainmentVolumeComponent::Deserialize(const nlohmann::json& json_)
{
    shape = static_cast<CONTAINMENT_SHAPE>(json_.value("Shape", static_cast<int>(shape)));
    targetMask = json_.value("TargetMask", targetMask);
    radius = json_.value("Radius", radius);
    halfHeight = json_.value("HalfHeight", halfHeight);
    targetRadius = json_.value("TargetRadius", targetRadius);
    constrainHeight = json_.value("ConstrainHeight", constrainHeight);
}

REGISTER_COMPONENT(ContainmentVolumeComponent, "ContainmentVolumeComponent")
