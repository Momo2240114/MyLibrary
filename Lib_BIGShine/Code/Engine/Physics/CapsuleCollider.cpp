#include "PrecompiledHeader.h"

#include "CapsuleCollider.h"

#include "BoxCollider.h"
#include "SphereCollider.h"
#include "Engine/Math/CollisionMath.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace
{
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

    physx::PxQuat CreateCapsuleRotation(const physx::PxVec3& direction_)
    {
        using namespace physx;
        const PxVec3 capsuleAxis_(1.0f, 0.0f, 0.0f);
        const float dot_ = (std::clamp)(capsuleAxis_.dot(direction_), -1.0f, 1.0f);
        if (dot_ <= -0.9999f)
        {
            return PxQuat(PxPi, PxVec3(0.0f, 1.0f, 0.0f));
        }
        if (dot_ >= 0.9999f)
        {
            return PxQuat(PxIdentity);
        }

        const PxVec3 cross_ = capsuleAxis_.cross(direction_);
        const float scale_ = std::sqrt((1.0f + dot_) * 2.0f);
        const float inverseScale_ = 1.0f / scale_;
        return PxQuat(
            cross_.x * inverseScale_,
            cross_.y * inverseScale_,
            cross_.z * inverseScale_,
            scale_ * 0.5f).getNormalized();
    }
}

void CapsuleCollider::Gui()
{
    ColliderComponent::Gui();

    ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, 10.0f);
    ImGui::DragFloat("Half Height", &halfHeight, 0.01f, 0.001f, 10.0f);

    ImGui::SeparatorText("Node Segment");
    const std::shared_ptr<GltfModelCommponennt> model_ = ResolveNodeModel();
    auto drawNodeCombo_ = [this, &model_](
        const char* label_,
        std::string& nodeName_,
        int& nodeIndex_)
        {
            const char* preview_ = nodeName_.empty() ? "(Transform)" : nodeName_.c_str();
            if (!ImGui::BeginCombo(label_, preview_)) return;

            if (ImGui::Selectable("(Transform)", nodeName_.empty()))
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
                    if (candidateName_.empty()) continue;
                    if (ImGui::Selectable(candidateName_.c_str(), candidateName_ == nodeName_))
                    {
                        nodeName_ = candidateName_;
                        nodeIndex_ = index_;
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("This GameObject has no GLTF model.");
            }
            ImGui::EndCombo();
        };

    drawNodeCombo_("Start Node", startNodeName, startNodeIndex);
    drawNodeCombo_("End Node", endNodeName, endNodeIndex);
    ImGui::TextDisabled("Both nodes are required. Missing nodes use the Transform capsule.");
}

void CapsuleCollider::Update(float deltaTime_)
{
    (void)deltaTime_;

    DirectX::XMFLOAT3 startPosition_{};
    DirectX::XMFLOAT3 endPosition_{};
    if (TryGetNodeSegment(startPosition_, endPosition_))
    {
        if (!shapeRenderEnabled) return;

        const float deltaX_ = endPosition_.x - startPosition_.x;
        const float deltaY_ = endPosition_.y - startPosition_.y;
        const float deltaZ_ = endPosition_.z - startPosition_.z;
        const float length_ = std::sqrt(
            deltaX_ * deltaX_ + deltaY_ * deltaY_ + deltaZ_ * deltaZ_);
        if (length_ <= 0.00001f) return;

        DirectX::XMFLOAT4X4 drawWorld_{};
        DirectX::XMStoreFloat4x4(
            &drawWorld_,
            CreateSegmentTransform(startPosition_, endPosition_));
        const auto scale_ = GetGameObject()->transform->GetWorldScale();
        const float worldRadius_ = radius * shapeSizeScale *
            (std::max)(std::abs(scale_.x), std::abs(scale_.z));
        GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawCapsule(
            drawWorld_, worldRadius_, length_, debugPrimitiveColor);
        return;
    }

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    const auto transform_ = GetGameObject()->transform;
    using namespace DirectX;
    const XMFLOAT4X4 worldFloat_ = transform_->GetWorld();
    const XMMATRIX world_ = XMLoadFloat4x4(&worldFloat_);
    const XMMATRIX offsetMatrix_ = XMMatrixTranslation(offset.x, offset.y, offset.z);
    XMFLOAT4X4 drawWorld_{};
    XMStoreFloat4x4(&drawWorld_, offsetMatrix_ * world_);

    const XMFLOAT3 scale_ = transform_->GetWorldScale();
    const float worldRadius_ = radius * shapeSizeScale * (std::max)(std::abs(scale_.x), std::abs(scale_.z));
    const float worldHeight_ = halfHeight * 2.0f * shapeSizeScale * std::abs(scale_.y);
    if (shapeRenderEnabled)
    {
        renderer_.DrawCapsule(drawWorld_, worldRadius_, worldHeight_, debugPrimitiveColor);
    }
}

void CapsuleCollider::Serialize(nlohmann::json& json_) const
{
    ColliderComponent::Serialize(json_);
    json_["Radius"] = radius;
    json_["HalfHeight"] = halfHeight;
    json_["StartNodeName"] = startNodeName;
    json_["EndNodeName"] = endNodeName;
}

void CapsuleCollider::Deserialize(const nlohmann::json& json_)
{
    ColliderComponent::Deserialize(json_);
    radius = json_.value("Radius", 0.5f);
    halfHeight = json_.value("HalfHeight", 1.0f);
    startNodeName = json_.value("StartNodeName", std::string{});
    endNodeName = json_.value("EndNodeName", std::string{});
    startNodeIndex = -1;
    endNodeIndex = -1;
}

void CapsuleCollider::OnPhysicsShapePrepared()
{
    DirectX::XMFLOAT3 startPosition_{};
    DirectX::XMFLOAT3 endPosition_{};
    if (TryGetNodeSegment(startPosition_, endPosition_))
    {
        UpdateNodeSegmentShape(startPosition_, endPosition_);
    }
}

std::shared_ptr<GltfModelCommponennt> CapsuleCollider::ResolveNodeModel()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    return owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
}

bool CapsuleCollider::ResolveNodeIndex(
    const std::shared_ptr<GltfModelCommponennt>& model_,
    const std::string& nodeName_,
    int& nodeIndex_)
{
    if (!model_ || nodeName_.empty())
    {
        nodeIndex_ = -1;
        return false;
    }

    const auto& nodes_ = model_->GetNodes();
    const bool validIndex_ = nodeIndex_ >= 0 &&
        nodeIndex_ < static_cast<int>(nodes_.size()) &&
        nodes_[nodeIndex_].name == nodeName_;
    if (!validIndex_) nodeIndex_ = model_->GetNodeIndex(nodeName_.c_str());
    return nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(nodes_.size());
}

bool CapsuleCollider::TryGetNodeSegment(
    DirectX::XMFLOAT3& startPosition_,
    DirectX::XMFLOAT3& endPosition_)
{
    const std::shared_ptr<GltfModelCommponennt> model_ = ResolveNodeModel();
    if (!ResolveNodeIndex(model_, startNodeName, startNodeIndex) ||
        !ResolveNodeIndex(model_, endNodeName, endNodeIndex))
    {
        return false;
    }

    const auto& nodes_ = model_->GetNodes();
    const auto& startTransform_ = nodes_[startNodeIndex].worldTransform;
    const auto& endTransform_ = nodes_[endNodeIndex].worldTransform;
    startPosition_ = { startTransform_._41, startTransform_._42, startTransform_._43 };
    endPosition_ = { endTransform_._41, endTransform_._42, endTransform_._43 };

    const float deltaX_ = endPosition_.x - startPosition_.x;
    const float deltaY_ = endPosition_.y - startPosition_.y;
    const float deltaZ_ = endPosition_.z - startPosition_.z;
    return deltaX_ * deltaX_ + deltaY_ * deltaY_ + deltaZ_ * deltaZ_ > 0.0000001f;
}

void CapsuleCollider::UpdateNodeSegmentShape(
    const DirectX::XMFLOAT3& startPosition_,
    const DirectX::XMFLOAT3& endPosition_)
{
    using namespace physx;
    if (!actor) return;

    PxShape* shape_ = nullptr;
    if (actor->getNbShapes() == 0) return;
    actor->getShapes(&shape_, 1);
    if (!shape_) return;

    const PxVec3 start_(startPosition_.x, startPosition_.y, startPosition_.z);
    const PxVec3 end_(endPosition_.x, endPosition_.y, endPosition_.z);
    const PxVec3 segment_ = end_ - start_;
    const float length_ = segment_.magnitude();
    if (length_ <= 0.00001f) return;

    const auto worldScale_ = GetGameObject()->transform->GetWorldScale();
    const float radiusScale_ = (std::max)(std::abs(worldScale_.x), std::abs(worldScale_.z));
    shape_->setGeometry(PxCapsuleGeometry(
        (std::max)(radius * shapeSizeScale * radiusScale_, 0.001f),
        (std::max)(length_ * 0.5f, 0.001f)));

    const PxTransform actorPose_ = actor->getGlobalPose();
    const PxVec3 worldCenter_ = (start_ + end_) * 0.5f;
    const PxVec3 localCenter_ = actorPose_.transformInv(worldCenter_);
    const PxVec3 localDirection_ = actorPose_.q.rotateInv(segment_ / length_);
    shape_->setLocalPose(PxTransform(
        localCenter_,
        CreateCapsuleRotation(localDirection_)));
}

Capsule CapsuleCollider::GetCapsuleData()
{
    DirectX::XMFLOAT3 startPosition_{};
    DirectX::XMFLOAT3 endPosition_{};
    Capsule capsule_{};
    if (TryGetNodeSegment(startPosition_, endPosition_))
    {
        capsule_.p0 = DirectX::XMLoadFloat3(&startPosition_);
        capsule_.p1 = DirectX::XMLoadFloat3(&endPosition_);
        const auto scale_ = GetGameObject()->transform->GetWorldScale();
        capsule_.radius = radius * shapeSizeScale *
            (std::max)(std::abs(scale_.x), std::abs(scale_.z));
        return capsule_;
    }

    const auto transform_ = GetGameObject()->transform;
    const DirectX::XMMATRIX world_ = transform_->GetWorldMatrix();
    const DirectX::XMVECTOR center_ = DirectX::XMVector3TransformCoord(
        DirectX::XMLoadFloat3(&offset), world_);
    const DirectX::XMVECTOR up_ = DirectX::XMVector3Normalize(world_.r[1]);
    const auto scale_ = transform_->GetWorldScale();
    const float worldHalfHeight_ = halfHeight * shapeSizeScale * std::abs(scale_.y);
    capsule_.p0 = DirectX::XMVectorAdd(center_, DirectX::XMVectorScale(up_, worldHalfHeight_));
    capsule_.p1 = DirectX::XMVectorSubtract(center_, DirectX::XMVectorScale(up_, worldHalfHeight_));
    capsule_.radius = radius * shapeSizeScale *
        (std::max)(std::abs(scale_.x), std::abs(scale_.z));
    return capsule_;
}

CollisionManifold CapsuleCollider::CheckCollision(std::shared_ptr<ColliderComponent> other_)
{
    CollisionManifold manifold_{};
    manifold_.isHit = false;

    const Capsule self_ = GetCapsuleData();
    switch (other_->GetColliderType())
    {
    case ColliderType::Sphere:
        return CollisionMath::CapsuleVsSphere(
            self_, static_cast<SphereCollider*>(other_.get())->GetSphereData());
    case ColliderType::Box:
        manifold_ = CollisionMath::OBBVsCapsule(
            static_cast<BoxCollider*>(other_.get())->CreateOBB(), self_);
        manifold_.normal = DirectX::XMVectorNegate(manifold_.normal);
        return manifold_;
    case ColliderType::Capsule:
        return CollisionMath::CapsuleVsCapsule(
            self_, static_cast<CapsuleCollider*>(other_.get())->GetCapsuleData());
    default:
        return manifold_;
    }
}
