#include "PrecompiledHeader.h"

#include "ContainmentVolumeComponent.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "PhysXSystem.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

using namespace physx;

namespace
{
    constexpr PxU32 PHYSICAL_ROLE = 0;
    constexpr PxU32 PHYSICS_RESPONSE_BIT = 0x80000000u;
    constexpr float FLOAT_EPSILON = 0.0001f;

    bool NearlyEqual(float a_, float b_)
    {
        return std::abs(a_ - b_) <= FLOAT_EPSILON;
    }

    bool NearlyEqual(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
    {
        return NearlyEqual(a_.x, b_.x)
            && NearlyEqual(a_.y, b_.y)
            && NearlyEqual(a_.z, b_.z);
    }

    bool BuildWorldPose(
        const std::shared_ptr<GameObject>& owner_,
        PxTransform& pose_,
        DirectX::XMFLOAT3& scale_)
    {
        // ObjectのWorld行列からPhysX Actor用のPoseとScaleを取り出す。
        // PhysX ShapeのサイズにはScaleを焼き込み、Actor Poseには位置/回転だけを渡す。
        if (!owner_ || !owner_->transform) return false;

        const DirectX::XMMATRIX world_ = owner_->transform->GetWorldMatrix();
        DirectX::XMVECTOR scaleVector_;
        DirectX::XMVECTOR rotationVector_;
        DirectX::XMVECTOR positionVector_;

        if (!DirectX::XMMatrixDecompose(
            &scaleVector_,
            &rotationVector_,
            &positionVector_,
            world_))
        {
            return false;
        }

        DirectX::XMFLOAT3 position_;
        DirectX::XMFLOAT4 rotation_;
        DirectX::XMStoreFloat3(&scale_, scaleVector_);
        DirectX::XMStoreFloat3(&position_, positionVector_);
        DirectX::XMStoreFloat4(&rotation_, rotationVector_);

        PxQuat quat_(rotation_.x, rotation_.y, rotation_.z, rotation_.w);
        if (!quat_.isSane())
        {
            quat_ = PxQuat(PxIdentity);
        }

        pose_ = PxTransform(PxVec3(position_.x, position_.y, position_.z), quat_.getNormalized());
        return pose_.isSane();
    }

    void AddCylinderMesh(
        std::vector<DirectX::XMFLOAT3>& vertices_,
        std::vector<uint32_t>& indices_,
        float radius_,
        float halfHeight_,
        int segments_,
        bool addCaps_)
    {
        // 閉じ込め用の円柱壁をTriangle Meshとして作る。
        // addCapsがtrueなら上下の面も張り、Volume外へ上下から抜けないようにする。
        vertices_.reserve(static_cast<size_t>(segments_) * 2 + 2);
        indices_.reserve(static_cast<size_t>(segments_) * (addCaps_ ? 12 : 6));

        for (int index_ = 0; index_ < segments_; ++index_)
        {
            const float angle_ = DirectX::XM_2PI * static_cast<float>(index_) / static_cast<float>(segments_);
            const float x_ = std::cos(angle_) * radius_;
            const float z_ = std::sin(angle_) * radius_;
            vertices_.push_back({ x_, -halfHeight_, z_ });
            vertices_.push_back({ x_, halfHeight_, z_ });
        }

        for (int index_ = 0; index_ < segments_; ++index_)
        {
            const uint32_t bottomCurrent_ = static_cast<uint32_t>(index_ * 2);
            const uint32_t topCurrent_ = bottomCurrent_ + 1;
            const uint32_t bottomNext_ = static_cast<uint32_t>(((index_ + 1) % segments_) * 2);
            const uint32_t topNext_ = bottomNext_ + 1;

            indices_.push_back(bottomCurrent_);
            indices_.push_back(bottomNext_);
            indices_.push_back(topCurrent_);

            indices_.push_back(bottomNext_);
            indices_.push_back(topNext_);
            indices_.push_back(topCurrent_);
        }

        if (!addCaps_) return;

        const uint32_t bottomCenter_ = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({ 0.0f, -halfHeight_, 0.0f });
        const uint32_t topCenter_ = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({ 0.0f, halfHeight_, 0.0f });

        for (int index_ = 0; index_ < segments_; ++index_)
        {
            const uint32_t bottomCurrent_ = static_cast<uint32_t>(index_ * 2);
            const uint32_t bottomNext_ = static_cast<uint32_t>(((index_ + 1) % segments_) * 2);
            const uint32_t topCurrent_ = bottomCurrent_ + 1;
            const uint32_t topNext_ = bottomNext_ + 1;

            indices_.push_back(bottomCenter_);
            indices_.push_back(bottomNext_);
            indices_.push_back(bottomCurrent_);

            indices_.push_back(topCenter_);
            indices_.push_back(topCurrent_);
            indices_.push_back(topNext_);
        }
    }
}
ContainmentVolumeComponent::~ContainmentVolumeComponent()
{
    // Component破棄時にPhysX Sceneへ残ったStatic Actorを必ず解放する。
    ReleasePhysicsVolume();
}

void ContainmentVolumeComponent::Start()
{
    // Scene開始時点で閉じ込め壁をPhysX Sceneへ登録する。
    RebuildPhysicsVolume();
}

void ContainmentVolumeComponent::Update(float deltaTime_)
{
    // UpdateではDebug形状の表示だけを行う。
    // 実際の閉じ込めはApplyContainmentでPhysX Actorを更新する流れに分けている。
    (void)deltaTime_;
    if (!showDebugShape) return;

    auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const DirectX::XMFLOAT3 scale_ = owner_->transform->GetWorldScale();
    const float worldRadius_ = radius * (std::max)(std::abs(scale_.x), std::abs(scale_.z));
    const float worldHeight_ = halfHeight * 2.0f * std::abs(scale_.y);

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    const DirectX::XMFLOAT4X4 world_ = owner_->transform->GetWorld();
    if (shape == CONTAINMENT_SHAPE::CAPSULE)
    {
        renderer_.DrawCapsule(world_, worldRadius_, worldHeight_, debugShapeColor);
        return;
    }

    renderer_.DrawCylinder(world_, worldRadius_, worldHeight_, debugShapeColor);
}

void ContainmentVolumeComponent::ApplyContainment()
{
    // GameObjectManagerの移動処理後に呼ばれ、Transform変更や設定変更をPhysX壁へ反映する。
    // 手動で対象Objectを押し戻すのではなく、Static Mesh壁として接触解決させる設計。
    auto owner_ = GetGameObject();
    if (!owner_) return;

    PxTransform pose_;
    DirectX::XMFLOAT3 scale_;
    if (!BuildWorldPose(owner_, pose_, scale_)) return;

    const bool settingsChanged_ =
        physicsDirty ||
        !actor ||
        cachedShape != shape ||
        !NearlyEqual(cachedRadius, radius) ||
        !NearlyEqual(cachedHalfHeight, halfHeight) ||
        cachedConstrainHeight != constrainHeight ||
        cachedSegments != segments ||
        !NearlyEqual(cachedScale, scale_);

    if (settingsChanged_)
    {
        // 形状、Scale、分割数などMesh自体が変わる項目はShapeの作り直しが必要。
        RebuildPhysicsVolume();
        return;
    }

    actor->setGlobalPose(pose_);
}

void ContainmentVolumeComponent::RebuildPhysicsVolume()
{
    // 現在の半径/高さ/ScaleからTriangle MeshをCookし、Static ActorとしてSceneへ追加する。
    // 既存Actorは必ず破棄してから作り直し、古い壁がSceneに残らないようにする。
    auto owner_ = GetGameObject();
    auto physics_ = PhysXSystem::GetPhysics();
    auto scene_ = PhysXSystem::GetScene();
    if (!owner_ || !physics_ || !scene_) return;

    PxTransform pose_;
    DirectX::XMFLOAT3 scale_;
    if (!BuildWorldPose(owner_, pose_, scale_)) return;

    ReleasePhysicsVolume();

    const float scaleRadius_ = (std::max)(std::abs(scale_.x), std::abs(scale_.z));
    const float volumeRadius_ = (std::max)(radius * scaleRadius_, 0.001f);
    const float volumeHalfHeight_ = (std::max)(halfHeight * std::abs(scale_.y), 0.001f);
    const int segments_ = std::clamp(segments, 8, 128);

    std::vector<DirectX::XMFLOAT3> vertices_;
    std::vector<uint32_t> indices_;
    AddCylinderMesh(vertices_, indices_, volumeRadius_, volumeHalfHeight_, segments_, constrainHeight);

    // Staticな閉じ込め壁なのでTriangle Meshを使う。動的に動かす用途ではなく、必要時に再構築する。
    auto mesh_ = PhysXSystem::CreateTriangleMesh(vertices_, indices_);
    if (!mesh_) return;

    actor = physics_->createRigidStatic(pose_);
    if (!actor)
    {
        mesh_->release();
        return;
    }

    actor->userData = nullptr;

    auto material_ = PhysXSystem::GetDefaultMaterial();
    if (!material_)
    {
        mesh_->release();
        ReleasePhysicsVolume();
        return;
    }

    auto shape_ = PxRigidActorExt::createExclusiveShape(
        *actor,
        PxTriangleMeshGeometry(mesh_),
        *material_);
    mesh_->release();

    if (!shape_)
    {
        ReleasePhysicsVolume();
        return;
    }

    shape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
    shape_->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);

    PxFilterData filterData_;
    // Wall LayerかつPhysics Responseありとして登録し、Characterや物理Colliderを押し返す壁にする。
    filterData_.word0 = static_cast<PxU32>(LayerType::Wall);
    filterData_.word1 = 0;
    filterData_.word2 = PHYSICAL_ROLE;
    const PxU32 ownerId_ = static_cast<PxU32>(reinterpret_cast<uintptr_t>(this) & 0x7FFFFFFF);
    filterData_.word3 = ownerId_ | PHYSICS_RESPONSE_BIT;
    shape_->setSimulationFilterData(filterData_);
    shape_->setQueryFilterData(filterData_);

    scene_->addActor(*actor);

    cachedShape = shape;
    // 再構築判定用に、PhysXへ反映した設定値をCacheする。
    cachedRadius = radius;
    cachedHalfHeight = halfHeight;
    cachedConstrainHeight = constrainHeight;
    cachedSegments = segments;
    cachedScale = scale_;
    physicsDirty = false;
}

void ContainmentVolumeComponent::ReleasePhysicsVolume()
{
    // ActorがSceneに登録済みならSceneから外してからreleaseする。
    if (!actor) return;

    if (auto scene_ = actor->getScene())
    {
        scene_->removeActor(*actor);
    }

    actor->userData = nullptr;
    actor->release();
    actor = nullptr;
}

void ContainmentVolumeComponent::MarkPhysicsDirty()
{
    // GUIやDeserializeで設定が変わった時、次回ApplyContainmentでRebuildさせる。
    physicsDirty = true;
}

void ContainmentVolumeComponent::Gui()
{
    // 閉じ込め壁の形状、半径、高さ、Debug表示をEditor上で調整する。
    // 変更時は即座にPhysXを触らずDirtyにし、通常Updateの流れで安全に再構築する。
    const char* shapeNames_[] = { "Cylinder", "Capsule" };
    int shapeIndex_ = static_cast<int>(shape);
    if (ImGui::Combo("Shape", &shapeIndex_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
    {
        shape = static_cast<CONTAINMENT_SHAPE>(shapeIndex_);
        MarkPhysicsDirty();
    }

    if (shape == CONTAINMENT_SHAPE::CAPSULE)
    {
        ImGui::TextDisabled("Capsule currently uses the same inward cylinder wall.");
    }

    if (ImGui::DragFloat("Radius", &radius, 0.05f, 0.01f, 1000.0f))
    {
        MarkPhysicsDirty();
    }
    if (ImGui::DragFloat("Half Height", &halfHeight, 0.05f, 0.01f, 1000.0f))
    {
        MarkPhysicsDirty();
    }

    if (ImGui::Checkbox("Constrain Height", &constrainHeight))
    {
        MarkPhysicsDirty();
    }
    if (ImGui::DragInt("Segments", &segments, 1.0f, 8, 128))
    {
        segments = std::clamp(segments, 8, 128);
        MarkPhysicsDirty();
    }

    ImGui::Separator();
    ImGui::Checkbox("Debug Shape", &showDebugShape);
    if (showDebugShape)
    {
        ImGui::ColorEdit4("Debug Shape Color", &debugShapeColor.x);
    }

    ImGui::Separator();
    ImGui::TextDisabled("PhysX wall mode does not manually move target objects.");
    ImGui::DragFloat("Legacy Target Radius", &targetRadius, 0.01f, 0.0f, 100.0f);
    ImGui::InputScalar("Legacy Target Mask", ImGuiDataType_U32, &targetMask);
}

void ContainmentVolumeComponent::Serialize(nlohmann::json& json_) const
{
    // 壁形状とDebug表示設定をScene JSONへ保存する。
    // PhysX ActorやCache値はRuntime状態なので保存しない。
    json_["Shape"] = static_cast<int>(shape);
    json_["TargetMask"] = targetMask;
    json_["Radius"] = radius;
    json_["HalfHeight"] = halfHeight;
    json_["TargetRadius"] = targetRadius;
    json_["ConstrainHeight"] = constrainHeight;
    json_["Segments"] = segments;
    json_["ShowDebugShape"] = showDebugShape;
    json_["DebugShapeColor"] = {
        debugShapeColor.x,
        debugShapeColor.y,
        debugShapeColor.z,
        debugShapeColor.w
    };
}

void ContainmentVolumeComponent::Deserialize(const nlohmann::json& json_)
{
    // 保存値を復元し、Mesh分割数などは安全範囲へ丸める。
    // 復元後はPhysX Actorを作り直す必要があるためDirtyにする。
    shape = static_cast<CONTAINMENT_SHAPE>(json_.value("Shape", static_cast<int>(shape)));
    targetMask = json_.value("TargetMask", targetMask);
    radius = json_.value("Radius", radius);
    halfHeight = json_.value("HalfHeight", halfHeight);
    targetRadius = json_.value("TargetRadius", targetRadius);
    constrainHeight = json_.value("ConstrainHeight", constrainHeight);
    segments = std::clamp(json_.value("Segments", segments), 8, 128);
    showDebugShape = json_.value("ShowDebugShape", showDebugShape);
    if (json_.contains("DebugShapeColor"))
    {
        debugShapeColor = {
            json_["DebugShapeColor"][0],
            json_["DebugShapeColor"][1],
            json_["DebugShapeColor"][2],
            json_["DebugShapeColor"][3]
        };
    }
    MarkPhysicsDirty();
}

REGISTER_COMPONENT(ContainmentVolumeComponent, "ContainmentVolumeComponent")
