#include "PrecompiledHeader.h"

#include "CapsuleCollider.h"

#include "BoxCollider.h"
#include "SphereCollider.h"
#include "Engine/Math/CollisionMath.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace
{
    // 2つのNode位置を結ぶCapsuleをDebug描画するため、線分方向をY軸にしたWorld行列を作る。
    // CapsuleCollider本体のTransformではなくBone/Node間の位置を基準にしたい時に使う。
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
        // PhysXのCapsuleはX軸方向が長軸なので、任意方向へ向ける回転を作る。
        // ほぼ同方向/反対方向の場合は外積が不安定になるため、特別扱いしている。
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

    void CollectParentModelObjects(
        const std::shared_ptr<GameObject>& owner_,
        std::vector<std::shared_ptr<GameObject>>& objects_)
    {
        // Colliderが武器や部位などの子Objectに付いている場合、
        // 親階層にあるModel ObjectをNode追従候補としてEditorに出す。
        if (!owner_ || !owner_->transform) return;

        std::shared_ptr<Transform> currentParent_ = owner_->transform->parent.lock();
        while (currentParent_)
        {
            std::shared_ptr<GameObject> parentObject_ = currentParent_->gameObject.lock();
            if (parentObject_ && parentObject_->GetComponent<GltfModelCommponennt>())
            {
                objects_.push_back(parentObject_);
            }
            currentParent_ = currentParent_->parent.lock();
        }
    }
}

void CapsuleCollider::Gui()
{
    // Capsuleの基本形状と、Model Node間に追従させるためのNode設定をEditorで調整する。
    // Nodeを指定しない場合は通常のTransform基準Capsuleとして扱う。
    ColliderComponent::Gui();

    ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, 10.0f);
    ImGui::DragFloat("Half Height", &halfHeight, 0.01f, 0.001f, 10.0f);

    ImGui::SeparatorText("Node Segment");
    ImGui::DragFloat("Node Segment Scale", &nodeSegmentScale, 0.01f, 0.1f, 5.0f);
    const std::shared_ptr<GltfModelCommponennt> model_ = ResolveNodeModel();
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    std::vector<std::shared_ptr<GameObject>> parentModelObjects_;
    CollectParentModelObjects(owner_, parentModelObjects_);

    std::string modelPreview_ = nodeModelObjectName.empty() ? "(Self)" : nodeModelObjectName;
    if (ImGui::BeginCombo("Node Model", modelPreview_.c_str()))
    {
        if (ImGui::Selectable("(Self)", nodeModelObjectName.empty()))
        {
            nodeModelObjectName.clear();
            startNodeIndex = -1;
            endNodeIndex = -1;
        }

        for (const std::shared_ptr<GameObject>& parentObject_ : parentModelObjects_)
        {
            if (!parentObject_) continue;
            const std::string& parentName_ = parentObject_->GetName();
            if (ImGui::Selectable(parentName_.c_str(), nodeModelObjectName == parentName_))
            {
                nodeModelObjectName = parentName_;
                startNodeIndex = -1;
                endNodeIndex = -1;
            }
        }

        ImGui::EndCombo();
    }

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
                ImGui::TextDisabled("Selected GameObject has no ready GLTF model.");
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

    // Node追従が成立する場合は、Transform基準ではなくStart/End Nodeを結ぶCapsuleをDebug描画する。
    // Boneに沿ったHitBoxやHurtBoxではこちらの経路が使われる。
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
    // Node追従できない場合は、ColliderComponentのWorld Transform + Offsetを使う通常Capsuleとして描画する。
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
    // 基本形状に加えて、Node追従に必要なModel Object名とNode名を保存する。
    // Node IndexはModel読み込み後に解決するCacheなので保存しない。
    ColliderComponent::Serialize(json_);
    json_["NodeSegmentScale"] = nodeSegmentScale;
    json_["Radius"] = radius;
    json_["HalfHeight"] = halfHeight;
    json_["NodeModelObjectName"] = nodeModelObjectName;
    json_["StartNodeName"] = startNodeName;
    json_["EndNodeName"] = endNodeName;
}

void CapsuleCollider::Deserialize(const nlohmann::json& json_)
{
    // 保存されたNode名を復元し、Index CacheはModel再読み込みに備えて無効化する。
    ColliderComponent::Deserialize(json_);
    nodeSegmentScale = std::clamp(json_.value("NodeSegmentScale", 1.0f), 0.1f, 5.0f);
    radius = json_.value("Radius", 0.5f);
    halfHeight = json_.value("HalfHeight", 1.0f);
    nodeModelObjectName = json_.value("NodeModelObjectName", std::string{});
    startNodeName = json_.value("StartNodeName", std::string{});
    endNodeName = json_.value("EndNodeName", std::string{});
    startNodeIndex = -1;
    endNodeIndex = -1;
}

void CapsuleCollider::OnPhysicsShapePrepared()
{
    // PhysX Shape作成時点でNode追従が有効なら、通常CapsuleではなくNode間CapsuleへShapeを更新する。
    // これによりDebug描画だけでなく、物理/Trigger判定側もBone位置に追従する。
    DirectX::XMFLOAT3 startPosition_{};
    DirectX::XMFLOAT3 endPosition_{};
    if (TryGetNodeSegment(startPosition_, endPosition_))
    {
        UpdateNodeSegmentShape(startPosition_, endPosition_);
    }
}

std::shared_ptr<GltfModelCommponennt> CapsuleCollider::ResolveNodeModel()
{
    // nodeModelObjectNameが指定されていればScene内Object名からModelを探し、
    // 空ならColliderを持つObject自身のModelを使う。未LoadならNode追従は一旦無効扱いにする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    std::shared_ptr<GltfModelCommponennt> model_;
    if (!nodeModelObjectName.empty() && owner_->manager)
    {
        const std::shared_ptr<GameObject> targetObject_ =
            owner_->GetManager().Find(nodeModelObjectName);
        if (targetObject_) model_ = targetObject_->GetComponent<GltfModelCommponennt>();
    }
    else
    {
        model_ = owner_->GetComponent<GltfModelCommponennt>();
    }

    if (model_)
    {
        model_->UpdateLoadState();
        if (!model_->IsModelReady()) return nullptr;
    }
    return model_;
}

bool CapsuleCollider::ResolveNodeIndex(
    const std::shared_ptr<GltfModelCommponennt>& model_,
    const std::string& nodeName_,
    int& nodeIndex_)
{
    // Node名からIndexを解決する。前回解決済みのIndexがまだ同じNodeを指す場合はCacheを使う。
    // Model再読み込みやNode名変更に備えて、名前が一致しない時だけ再検索する。
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
    // Start/End NodeのWorld位置を取得できた時だけ、Node追従Capsuleとして扱う。
    // どちらか片方でも欠ける場合はfalseを返し、通常Transform基準CapsuleへFallbackする。
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
    // 物理形状・Debug・CPU判定が同じ拡張済みの終点を使用する。
    endPosition_ = { startPosition_.x + (endPosition_.x - startPosition_.x) * nodeSegmentScale,
        startPosition_.y + (endPosition_.y - startPosition_.y) * nodeSegmentScale,
        startPosition_.z + (endPosition_.z - startPosition_.z) * nodeSegmentScale };

    const float deltaX_ = endPosition_.x - startPosition_.x;
    const float deltaY_ = endPosition_.y - startPosition_.y;
    const float deltaZ_ = endPosition_.z - startPosition_.z;
    return deltaX_ * deltaX_ + deltaY_ * deltaY_ + deltaZ_ * deltaZ_ > 0.0000001f;
}

void CapsuleCollider::UpdateNodeSegmentShape(
    const DirectX::XMFLOAT3& startPosition_,
    const DirectX::XMFLOAT3& endPosition_)
{
    // PhysX Shapeを、Start/End Nodeを結ぶCapsuleへ更新する。
    // Actor自体のPoseはObject Transform基準なので、NodeのWorld中心と方向をActor Localへ変換してShape LocalPoseへ入れる。
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
    // CollisionStepSystemが手動判定で使うCapsuleデータを作る。
    // Node追従が有効ならBone間Capsule、無効ならTransform基準Capsuleを返す。
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
    // 他Colliderの型に応じてCollisionMathへ委譲する。
    // BoxVsCapsuleは引数順で法線向きが逆になるため、Capsule側から見た法線へ反転する。
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
