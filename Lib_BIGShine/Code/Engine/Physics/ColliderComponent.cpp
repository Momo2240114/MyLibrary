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
    if (!actor && !shapeMaterial) return;

    // PhysX本体の終了後はactor/materialも既に無効なので参照しない。
    if (!PhysXSystem::GetPhysics())
    {
        actor = nullptr;
        shapeMaterial = nullptr;
        return;
    }

    if (actor)
    {
        if (auto scene_ = actor->getScene())
        {
            scene_->removeActor(*actor);
        }
        actor->userData = nullptr;
        actor->release();
        actor = nullptr;
    }

    if (shapeMaterial)
    {
        shapeMaterial->release();
        shapeMaterial = nullptr;
    }
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
        ApplyDynamicActorSettings(*dynamicActor_);
        PxRigidBodyExt::setMassAndUpdateInertia(*dynamicActor_, std::max<float>(mass, 1.0f));
        actor = dynamicActor_;
    }

    if (!actor) return;

    actor->userData = this;
    UpdatePhysXShape();
    actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
    scene_->addActor(*actor);
    CaptureTransformSnapshot();
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
        auto material_ = GetShapeMaterial();
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

            const DirectX::XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();
            DirectX::XMVECTOR ownerWorldDeterminant_;
            const DirectX::XMMATRIX actorLocalFromWorld_ =
                DirectX::XMMatrixInverse(&ownerWorldDeterminant_, ownerWorld_);

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
                    position_ = DirectX::XMVector3TransformCoord(position_, actorLocalFromWorld_);
                    DirectX::XMFLOAT3 transformedPosition_;
                    DirectX::XMStoreFloat3(&transformedPosition_, position_);
                    vertices_.push_back(transformedPosition_);
                }
                for (const auto index_ : modelMesh_.indices) indices_.push_back(vertexOffset_ + index_);
            }

            const PxMeshScale meshScale_(PxVec3(1.0f, 1.0f, 1.0f));
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

void ColliderComponent::SyncFromPhysics(float dt_)
{
    if (!actor || actorIsStatic || !physicsControlsTransform) return;

    auto owner_ = GetGameObject();
    auto dynamicActor_ = actor->is<PxRigidDynamic>();
    if (!owner_ || !dynamicActor_) return;

    if (HasExternalTransformChange())
    {
        PushTransformToPhysics();
        CaptureTransformSnapshot();
        return;
    }

    const PxTransform pose_ = dynamicActor_->getGlobalPose();
    owner_->transform->localPosition = { pose_.p.x, pose_.p.y, pose_.p.z };
    const PxVec3 velocity_ = dynamicActor_->getLinearVelocity();
    velocity = { velocity_.x, velocity_.y, velocity_.z };
    characterGroundedForStep = collisionRole == CollisionRole::CHARACTER && isGrounded;

    if (characterGroundedForStep && velocity.y > 0.0f)
    {
        velocity.y = 0.0f;
        dynamicActor_->setLinearVelocity(PxVec3(velocity.x, 0.0f, velocity.z));
    }

    if (collisionRole == CollisionRole::CHARACTER &&
        characterGroundedForStep &&
        stopCharacterSlideWhenNoInput)
    {
        const float horizontalSpeedSq_ =
            velocity.x * velocity.x +
            velocity.z * velocity.z;

        const float stopThresholdSq_ =
            characterSlideStopThreshold * characterSlideStopThreshold;

        if (horizontalSpeedSq_ <= stopThresholdSq_)
        {
            velocity.x = 0.0f;
            velocity.z = 0.0f;

            dynamicActor_->setLinearVelocity(
                PxVec3(0.0f, velocity.y, 0.0f)
            );
        }
    }

    const float velocityLengthSq_ =
        velocity.x * velocity.x +
        velocity.y * velocity.y +
        velocity.z * velocity.z;

    const float thresholdSq_ =
        rotateVelocityThreshold * rotateVelocityThreshold;

    if (velocityRotationMode != VelocityRotationMode::NONE &&
        velocityLengthSq_ >= thresholdSq_)
    {
        if (velocityRotationMode == VelocityRotationMode::YAW_ONLY)
        {
            owner_->transform->SetForwardYawOnly(
                { velocity.x, 0.0f, velocity.z },
                dt_,
                10
            );
        }
        else if (velocityRotationMode == VelocityRotationMode::FULL_3D)
        {
            owner_->transform->SetForward(
                { velocity.x, velocity.y, velocity.z }
            );
        }
    }

    owner_->transform->MarkDirty();
    CaptureTransformSnapshot();
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
    CaptureTransformSnapshot();

    if (auto dynamicActor_ = actor->is<PxRigidDynamic>())
    {
        dynamicActor_->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !physicsControlsTransform);
        const bool disableCharacterGroundGravity_ =
            collisionRole == CollisionRole::CHARACTER &&
            characterGroundedForStep &&
            velocity.y <= 0.0f;
        dynamicActor_->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !useGravity || disableCharacterGroundGravity_);
        ApplyDynamicActorSettings(*dynamicActor_);
        if (physicsControlsTransform)
        {
            if (disableCharacterGroundGravity_)
            {
                velocity.y = 0.0f;
            }
            dynamicActor_->setLinearVelocity(PxVec3(velocity.x, velocity.y, velocity.z));
            if (useGravity && !disableCharacterGroundGravity_ && gravityScale != 1.0f)
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

void ColliderComponent::ApplyDynamicActorSettings(PxRigidDynamic& actor_)
{
    if (collisionRole != CollisionRole::CHARACTER)
    {
        actor_.setRigidDynamicLockFlags(PxRigidDynamicLockFlags());
        return;
    }

    actor_.setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
    actor_.setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, true);
    actor_.setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z, true);
    actor_.setAngularVelocity(PxVec3(0.0f, 0.0f, 0.0f));
    actor_.setAngularDamping(100.0f);
    actor_.setLinearDamping(0.0f);
}

PxMaterial* ColliderComponent::GetShapeMaterial()
{
    if (collisionRole != CollisionRole::CHARACTER)
    {
        return PhysXSystem::GetDefaultMaterial();
    }

    if (!shapeMaterial)
    {
        auto physics_ = PhysXSystem::GetPhysics();
        if (!physics_) return nullptr;

        shapeMaterial = physics_->createMaterial(0.0f, 0.0f, 0.0f);
        shapeMaterial->setFrictionCombineMode(PxCombineMode::eMIN);
        shapeMaterial->setRestitutionCombineMode(PxCombineMode::eMIN);
    }

    return shapeMaterial;
}

bool ColliderComponent::HasExternalTransformChange()
{
    if (!hasTransformSnapshot) return false;

    auto owner_ = GetGameObject();
    if (!owner_) return false;

    constexpr float epsilon_ = 0.0001f;
    const auto angleDifference_ = [](float angleA_, float angleB_)
        {
            return std::abs(std::remainder(angleA_ - angleB_, 360.0f));
        };
    const auto& position_ = owner_->transform->localPosition;
    const auto& rotation_ = owner_->transform->localRotation;
    return std::abs(position_.x - lastTransformPosition.x) > epsilon_
        || std::abs(position_.y - lastTransformPosition.y) > epsilon_
        || std::abs(position_.z - lastTransformPosition.z) > epsilon_
        || angleDifference_(rotation_.x, lastTransformRotation.x) > epsilon_
        || angleDifference_(rotation_.y, lastTransformRotation.y) > epsilon_
        || angleDifference_(rotation_.z, lastTransformRotation.z) > epsilon_;
}

void ColliderComponent::CaptureTransformSnapshot()
{
    auto owner_ = GetGameObject();
    if (!owner_) return;

    lastTransformPosition = owner_->transform->localPosition;
    lastTransformRotation = owner_->transform->localRotation;
    hasTransformSnapshot = true;
}

void ColliderComponent::PushTransformToPhysics()
{
    if (!actor) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    const auto transform_ = owner_->transform;
    actor->setGlobalPose(PxTransform(
        PxVec3(transform_->localPosition.x, transform_->localPosition.y, transform_->localPosition.z),
        EulerToPxQuat(transform_->localRotation)));
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


    ImGui::Separator();

    const char* containmentModeNames_[] =
    {
        "None",
        "Keep Inside"
    };

    int containmentModeIndex_ = static_cast<int>(containmentMode);
    if (ImGui::Combo(
        "Containment Mode",
        &containmentModeIndex_,
        containmentModeNames_,
        IM_ARRAYSIZE(containmentModeNames_)))
    {
        containmentMode =
            static_cast<CollisionContainmentMode>(containmentModeIndex_);
    }



    const char* velocityRotationModeNames_[] =
    {
        "None",
        "Yaw Only",
        "Full 3D"
    };

    int velocityRotationModeIndex_ = static_cast<int>(velocityRotationMode);
    if (ImGui::Combo(
        "Velocity Rotation",
        &velocityRotationModeIndex_,
        velocityRotationModeNames_,
        IM_ARRAYSIZE(velocityRotationModeNames_)))
    {
        velocityRotationMode =
            static_cast<VelocityRotationMode>(velocityRotationModeIndex_);
    }

    ImGui::TextDisabled("None: no rotation. Yaw Only: character-style Y rotation. Full 3D: bullet/missile-style rotation.");

    if (velocityRotationMode != VelocityRotationMode::NONE)
    {
        ImGui::DragFloat(
            "Velocity Rotate Threshold",
            &rotateVelocityThreshold,
            0.001f,
            0.0f,
            10.0f
        );
    }

    if (HasFixedPhysicsBehavior())
    {
        ImGui::TextDisabled("Detection only: no gravity, physical response, or Transform control.");
    }
    else
    {
        ImGui::Checkbox("Physical Response", &usePhysicsResponse);
        ImGui::TextDisabled("OFF: overlap events only; objects are never pushed.");
        ImGui::Checkbox("Physics Controls Transform", &physicsControlsTransform);
        ImGui::TextDisabled("ON: PhysX moves it. Inspector/script edits teleport the PhysX actor.");
        ImGui::Checkbox("Static Body", &isStatic);
        ImGui::Checkbox("Trigger Shape", &isTrigger);
        ImGui::Separator();
        ImGui::Checkbox("Use Gravity", &useGravity);
        ImGui::TextDisabled("Gravity defaults to OFF.");
    }

    ImGui::Checkbox("Debug Shape", &shapeRenderEnabled);
    ImGui::Text("Touching: %s", m_IsHitFlag ? "Yes" : "No");

    if (useGravity) {
        ImGui::DragFloat("Gravity Scale", &gravityScale, 0.1f, 0.0f, 10.0f);
    }
    ImGui::Separator();
    ImGui::DragFloat3("Offset", &offset.x, 0.01f);
    ImGui::DragFloat("mass", &mass, 0.01f);
    ImGui::ColorEdit3("PrimitiveColor", &debugPrimitiveColor.x);

    if (previousStatic_ != isStatic || previousTrigger_ != isTrigger
        || previousPhysicsControlsTransform_ != physicsControlsTransform)
    {
        RecreatePhysXActor();
    }
}
