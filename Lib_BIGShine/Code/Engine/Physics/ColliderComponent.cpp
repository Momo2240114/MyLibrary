#include "ColliderComponent.h"
#include "Collision.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "CylinderCollider.h"
#include "MeshCollider.h"
#include "PhysXSystem.h"
#include "PhysXMath.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>

using namespace physx;

ColliderComponent::~ColliderComponent()
{
    ReleasePhysXActor();
}

void ColliderComponent::Start()
{
    CreatePhysXActor();
}

void ColliderComponent::ReleasePhysXActor()
{
    if (!actor) return;

    if (auto scene_ = actor->getScene())
    {
        scene_->removeActor(*actor);
    }
    actor->userData = nullptr;
    actor->release();
    actor = nullptr;
}

void ColliderComponent::RecreatePhysXActor()
{
    ReleasePhysXActor();
    CreatePhysXActor();
}

void ColliderComponent::CreatePhysXActor()
{
    auto owner_ = GetGameObject();
    auto physics_ = PhysXSystem::GetPhysics();
    auto scene_ = PhysXSystem::GetScene();
    if (!owner_ || !physics_ || !scene_) return;

    auto transform_ = owner_->transform;
    PxTransform pose_(
        PxVec3(transform_->localPosition.x, transform_->localPosition.y, transform_->localPosition.z),
        EulerToPxQuat(transform_->localRotation));

    if (GetColliderType() == ColliderType::Mesh && !static_cast<MeshCollider*>(this)->convex)
    {
        isStatic = true;
    }

    actorIsStatic = isStatic;
    if (isStatic)
    {
        actor = physics_->createRigidStatic(pose_);
    }
    else
    {
        auto dynamicActor_ = physics_->createRigidDynamic(pose_);
        dynamicActor_->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !physicsControlsTransform);
        PxRigidBodyExt::setMassAndUpdateInertia(*dynamicActor_, std::max<float>(mass, 1.0f));
        actor = dynamicActor_;
    }

    if (!actor) return;

    actor->userData = this;
    UpdatePhysXShape();
    actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
    scene_->addActor(*actor);
}

void ColliderComponent::UpdatePhysXShape()
{
    if (!actor) return;

    PxShape* shape_ = nullptr;
    if (actor->getNbShapes() > 0)
    {
        actor->getShapes(&shape_, 1);
    }

    auto owner_ = GetGameObject();
    if (!owner_) return;

    auto scale_ = owner_->transform->GetWorldScale();
    if (!shape_)
    {
        auto material_ = PhysXSystem::GetDefaultMaterial();
        if (!material_) return;

        switch (GetColliderType())
        {
        case ColliderType::Box:
        {
            auto box_ = static_cast<BoxCollider*>(this);
            shape_ = PxRigidActorExt::createExclusiveShape(
                *actor,
                PxBoxGeometry(
                    std::max<float>(box_->halfSize.x * scale_.x, 0.001f),
                    std::max<float>(box_->halfSize.y * scale_.y, 0.001f),
                    std::max<float>(box_->halfSize.z * scale_.z, 0.001f)),
                *material_);
            break;
        }
        case ColliderType::Sphere:
        {
            auto sphere_ = static_cast<SphereCollider*>(this);
            shape_ = PxRigidActorExt::createExclusiveShape(
                *actor,
                PxSphereGeometry(std::max<float>(sphere_->radius * scale_.x, 0.001f)),
                *material_);
            break;
        }
        case ColliderType::Capsule:
        {
            auto capsule_ = static_cast<CapsuleCollider*>(this);
            shape_ = PxRigidActorExt::createExclusiveShape(
                *actor,
                PxCapsuleGeometry(
                    std::max<float>(capsule_->radius * scale_.x, 0.001f),
                    std::max<float>(capsule_->halfHeight * scale_.y, 0.001f)),
                *material_);
            break;
        }
        case ColliderType::Cylinder:
        {
            auto cylinder_ = static_cast<CylinderCollider*>(this);
            std::vector<DirectX::XMFLOAT3> vertices_;
            const int segments_ = std::clamp(cylinder_->segments, 6, 64);
            vertices_.reserve(static_cast<size_t>(segments_) * 2);
            for (int index_ = 0; index_ < segments_; ++index_)
            {
                const float angle_ = DirectX::XM_2PI * static_cast<float>(index_) / static_cast<float>(segments_);
                const float x_ = std::cos(angle_) * cylinder_->radius;
                const float z_ = std::sin(angle_) * cylinder_->radius;
                vertices_.push_back({ x_, cylinder_->halfHeight, z_ });
                vertices_.push_back({ x_, -cylinder_->halfHeight, z_ });
            }

            auto mesh_ = PhysXSystem::CreateConvexMesh(vertices_);
            if (mesh_)
            {
                shape_ = PxRigidActorExt::createExclusiveShape(
                    *actor,
                    PxConvexMeshGeometry(mesh_, PxMeshScale(PxVec3(scale_.x, scale_.y, scale_.z))),
                    *material_);
                mesh_->release();
            }
            break;
        }
        case ColliderType::Mesh:
        {
            auto meshCollider_ = static_cast<MeshCollider*>(this);
            auto model_ = owner_->GetComponent<GltfModelCommponennt>();
            if (!model_) break;

            std::vector<DirectX::XMFLOAT3> vertices_;
            std::vector<uint32_t> indices_;
            for (const auto& modelMesh_ : model_->GetMeshes())
            {
                const uint32_t vertexOffset_ = static_cast<uint32_t>(vertices_.size());
                DirectX::XMMATRIX meshTransform_ = DirectX::XMMatrixIdentity();
                if (modelMesh_.node)
                {
                    meshTransform_ = DirectX::XMLoadFloat4x4(&modelMesh_.node->globalTransform);
                }
                for (const auto& vertex_ : modelMesh_.vertices)
                {
                    DirectX::XMVECTOR position_ = DirectX::XMLoadFloat3(&vertex_.position);
                    position_ = DirectX::XMVector3TransformCoord(position_, meshTransform_);
                    DirectX::XMFLOAT3 transformedPosition_;
                    DirectX::XMStoreFloat3(&transformedPosition_, position_);
                    vertices_.push_back(transformedPosition_);
                }
                for (const auto index_ : modelMesh_.indices) indices_.push_back(vertexOffset_ + index_);
            }

            const PxMeshScale meshScale_(PxVec3(scale_.x, scale_.y, scale_.z));
            if (meshCollider_->convex)
            {
                auto mesh_ = PhysXSystem::CreateConvexMesh(vertices_);
                if (mesh_)
                {
                    shape_ = PxRigidActorExt::createExclusiveShape(
                        *actor, PxConvexMeshGeometry(mesh_, meshScale_), *material_);
                    mesh_->release();
                }
            }
            else
            {
                auto mesh_ = PhysXSystem::CreateTriangleMesh(vertices_, indices_);
                if (mesh_)
                {
                    shape_ = PxRigidActorExt::createExclusiveShape(
                        *actor, PxTriangleMeshGeometry(mesh_, meshScale_), *material_);
                    mesh_->release();
                }
            }
            break;
        }
        }
    }
    else
    {
        switch (GetColliderType())
        {
        case ColliderType::Box:
        {
            auto box_ = static_cast<BoxCollider*>(this);
            shape_->setGeometry(PxBoxGeometry(
                std::max<float>(box_->halfSize.x * scale_.x, 0.001f),
                std::max<float>(box_->halfSize.y * scale_.y, 0.001f),
                std::max<float>(box_->halfSize.z * scale_.z, 0.001f)));
            break;
        }
        case ColliderType::Sphere:
        {
            auto sphere_ = static_cast<SphereCollider*>(this);
            shape_->setGeometry(PxSphereGeometry(std::max<float>(sphere_->radius * scale_.x, 0.001f)));
            break;
        }
        case ColliderType::Capsule:
        {
            auto capsule_ = static_cast<CapsuleCollider*>(this);
            shape_->setGeometry(PxCapsuleGeometry(
                std::max<float>(capsule_->radius * scale_.x, 0.001f),
                std::max<float>(capsule_->halfHeight * scale_.y, 0.001f)));
            break;
        }
        case ColliderType::Cylinder:
        case ColliderType::Mesh:
        {
            if (shape_->getGeometry().getType() == PxGeometryType::eCONVEXMESH)
            {
                auto convexGeometry_ = static_cast<const PxConvexMeshGeometry&>(shape_->getGeometry());
                convexGeometry_.scale = PxMeshScale(PxVec3(scale_.x, scale_.y, scale_.z));
                shape_->setGeometry(convexGeometry_);
                break;
            }

            if (shape_->getGeometry().getType() == PxGeometryType::eTRIANGLEMESH)
            {
                auto triangleGeometry_ = static_cast<const PxTriangleMeshGeometry&>(shape_->getGeometry());
                triangleGeometry_.scale = PxMeshScale(PxVec3(scale_.x, scale_.y, scale_.z));
                shape_->setGeometry(triangleGeometry_);
            }
            break;
        }
        }
    }

    if (!shape_) return;

    PxQuat shapeRotation_ = PxQuat(PxIdentity);
    if (GetColliderType() == ColliderType::Capsule)
    {
        shapeRotation_ = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
    }
    shape_->setLocalPose(PxTransform(PxVec3(offset.x, offset.y, offset.z), shapeRotation_));
    shape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
    shape_->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);

    PxFilterData filterData_;
    filterData_.word0 = static_cast<PxU32>(owner_->layer);
    filterData_.word1 = owner_->mask;
    filterData_.word2 = static_cast<PxU32>(collisionRole);
    const PxU32 ownerId_ = static_cast<PxU32>(reinterpret_cast<uintptr_t>(owner_.get()) & 0x7FFFFFFF);
    filterData_.word3 = ownerId_ | (usePhysicsResponse ? 0x80000000u : 0u);
    shape_->setSimulationFilterData(filterData_);
}

void ColliderComponent::SyncFromPhysics()
{
    if (!actor || actorIsStatic || !physicsControlsTransform) return;

    auto owner_ = GetGameObject();
    auto dynamicActor_ = actor->is<PxRigidDynamic>();
    if (!owner_ || !dynamicActor_) return;

    const PxTransform pose_ = dynamicActor_->getGlobalPose();
    owner_->transform->localPosition = { pose_.p.x, pose_.p.y, pose_.p.z };
    const PxVec3 velocity_ = dynamicActor_->getLinearVelocity();
    velocity = { velocity_.x, velocity_.y, velocity_.z };
    owner_->transform->MarkDirty();
}

void ColliderComponent::PreparePhysicsStep()
{
    if (!actor || actorIsStatic != isStatic)
    {
        RecreatePhysXActor();
    }
    if (!actor) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
    if (!enabled) return;

    UpdatePhysXShape();

    auto transform_ = owner_->transform;
    PxTransform pose_(
        PxVec3(transform_->localPosition.x, transform_->localPosition.y, transform_->localPosition.z),
        EulerToPxQuat(transform_->localRotation));
    actor->setGlobalPose(pose_);

    if (auto dynamicActor_ = actor->is<PxRigidDynamic>())
    {
        dynamicActor_->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !physicsControlsTransform);
        dynamicActor_->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !useGravity);
        if (physicsControlsTransform)
        {
            dynamicActor_->setLinearVelocity(PxVec3(velocity.x, velocity.y, velocity.z));
            if (useGravity && gravityScale != 1.0f)
            {
                dynamicActor_->addForce(PxVec3(0.0f, -9.8f * (gravityScale - 1.0f), 0.0f), PxForceMode::eACCELERATION);
            }
        }
        else
        {
            dynamicActor_->setKinematicTarget(pose_);
        }
        PxRigidBodyExt::setMassAndUpdateInertia(*dynamicActor_, std::max<float>(mass, 1.0f));
    }
}

void ColliderComponent::ApplyRoleDefaults()
{
    useGravity = false;
    physicsControlsTransform = false;

    switch (collisionRole)
    {
    case CollisionRole::PHYSICAL:
        usePhysicsResponse = true;
        isTrigger = false;
        break;
    case CollisionRole::CHARACTER:
        usePhysicsResponse = true;
        physicsControlsTransform = true;
        isTrigger = false;
        break;
    case CollisionRole::HIT_BOX:
    case CollisionRole::HURT_BOX:
        usePhysicsResponse = false;
        isTrigger = false;
        break;
    case CollisionRole::SENSOR:
        usePhysicsResponse = false;
        isTrigger = true;
        break;
    }
}

void ColliderComponent::Gui()
{
    const bool previousStatic_ = isStatic;
    const bool previousTrigger_ = isTrigger;
    const bool previousPhysicsControlsTransform_ = physicsControlsTransform;

    const char* roleNames_[] = { "Physical", "Character", "HitBox", "HurtBox", "Sensor" };
    int roleIndex_ = static_cast<int>(collisionRole);
    if (HasFixedCollisionRole())
    {
        ImGui::Text("Purpose: %s (Fixed)", roleNames_[roleIndex_]);
    }
    else if (ImGui::Combo("Purpose", &roleIndex_, roleNames_, IM_ARRAYSIZE(roleNames_)))
    {
        collisionRole = static_cast<CollisionRole>(roleIndex_);
        ApplyRoleDefaults();
        RecreatePhysXActor();
    }
    ImGui::TextDisabled("Physical/Character: body collision. HitBox/HurtBox/Sensor: detection only.");

    if (HasFixedPhysicsBehavior())
    {
        ImGui::TextDisabled("Detection only: no gravity, physical response, or Transform control.");
    }
    else
    {
        ImGui::Checkbox("Physical Response", &usePhysicsResponse);
        ImGui::TextDisabled("OFF: overlap events only; objects are never pushed.");
        ImGui::Checkbox("Physics Controls Transform", &physicsControlsTransform);
        ImGui::TextDisabled("OFF: Inspector/scripts control Transform. ON: PhysX moves it.");
        ImGui::Checkbox("Static Body", &isStatic);
        ImGui::Checkbox("Trigger Shape", &isTrigger);
        ImGui::Separator();
        ImGui::Checkbox("Use Gravity", &useGravity);
        ImGui::TextDisabled("Gravity defaults to OFF.");
    }

    ImGui::Checkbox("Debug Shape", &_ShapeRenderFlg);
    ImGui::Text("Touching: %s", m_IsHitFlag ? "Yes" : "No");

    if (useGravity) {
        ImGui::DragFloat("Gravity Scale", &gravityScale, 0.1f, 0.0f, 10.0f);
    }
    ImGui::Separator();
    ImGui::DragFloat3("Offset", &offset.x, 0.01f);
    ImGui::ColorEdit3("PrimitiveColor", &debugPrimitiveColor.x);

    if (previousStatic_ != isStatic || previousTrigger_ != isTrigger
        || previousPhysicsControlsTransform_ != physicsControlsTransform)
    {
        RecreatePhysXActor();
    }
}
