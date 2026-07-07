#include "ColliderComponent.h"
#include "Collision.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "CapsuleCollider.h"
#include "CylinderCollider.h"
#include "MeshCollider.h"
#include "CharacterCollider.h"
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
    ReleaseCharacterController();

    if (!actor && !shapeMaterial) return;

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

void ColliderComponent::CreateCharacterController()
{
    auto owner_ = GetGameObject();
    auto physics_ = PhysXSystem::GetPhysics();
    auto scene_ = PhysXSystem::GetScene();
    auto controllerManager_ = PhysXSystem::GetControllerManager();

    if (!owner_ || !physics_ || !scene_ || !controllerManager_) return;

    auto capsule_ = dynamic_cast<CapsuleCollider*>(this);
    if (!capsule_) return;

    auto transform_ = owner_->transform;
    if (!transform_) return;

    auto scale_ = transform_->GetWorldScale();
    scale_.x *= shapeSizeScale;
    scale_.y *= shapeSizeScale;
    scale_.z *= shapeSizeScale;

    auto material_ = GetShapeMaterial();
    if (!material_) return;

    physx::PxCapsuleControllerDesc desc_;
    desc_.material = material_;
    desc_.position = physx::PxExtendedVec3(
        transform_->localPosition.x + offset.x,
        transform_->localPosition.y + offset.y,
        transform_->localPosition.z + offset.z
    );

    desc_.radius = std::max<float>(capsule_->radius * scale_.x, 0.001f);

    // PxCapsuleControllerDesc::height は「円柱部分の高さ」。
    // 既存 CapsuleCollider の halfHeight が PhysX PxCapsuleGeometry の halfHeight と同じ意味なら *2。
    desc_.height = std::max<float>(capsule_->halfHeight * 2.0f * scale_.y, 0.001f);

    desc_.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
    const auto characterCollider_ = dynamic_cast<CharacterCollider*>(this);
    desc_.slopeLimit = 0.707f;      // 約45度
    desc_.stepOffset = characterCollider_ ? characterCollider_->GetStepOffset() : 0.3f;
    desc_.contactOffset = characterCollider_ ? characterCollider_->GetContactOffset() : 0.05f;
    desc_.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
    desc_.behaviorCallback = PhysXSystem::GetCharacterControllerBehaviorCallback();
    desc_.density = std::max<float>(mass, 1.0f);
    desc_.scaleCoeff = 0.8f;
    desc_.volumeGrowth = 1.5f;

    if (!desc_.isValid()) return;

    characterController = controllerManager_->createController(desc_);
    if (!characterController) return;
    characterController->setUserData(this);

    if (auto controllerActor_ = characterController->getActor())
    {
        controllerActor_->userData = this;

        physx::PxShape* shape_ = nullptr;
        if (controllerActor_->getNbShapes() > 0)
        {
            controllerActor_->getShapes(&shape_, 1);
        }

        if (shape_)
        {
            auto filterData_ = physx::PxFilterData();
            filterData_.word0 = static_cast<physx::PxU32>(owner_->layer);
            filterData_.word1 = owner_->mask;
            filterData_.word2 = static_cast<physx::PxU32>(collisionRole);

            const physx::PxU32 ownerId_ =
                static_cast<physx::PxU32>(
                    reinterpret_cast<uintptr_t>(owner_.get()) & 0x7FFFFFFF
                    );

            filterData_.word3 = ownerId_ | 0x80000000u;

            shape_->setSimulationFilterData(filterData_);
            shape_->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
            shape_->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        }
    }

    physicsControlsTransform = true;
    isTrigger = false;
    usePhysicsResponse = true;

    CaptureTransformSnapshot();
}

void ColliderComponent::ReleaseCharacterController()
{
    if (characterController)
    {
        if (auto controllerActor_ = characterController->getActor())
        {
            controllerActor_->userData = nullptr;
        }

        characterController->release();
        characterController = nullptr;
    }
}

bool ColliderComponent::UsesCharacterController() const
{
    return collisionRole == CollisionRole::CHARACTER &&
        GetColliderType() == ColliderType::Capsule;
}

void ColliderComponent::CreatePhysXActor()
{
    if (UsesCharacterController())
    {
        CreateCharacterController();
        return;
    }

    if (collisionRole == CollisionRole::HIT_BOX ||
        collisionRole == CollisionRole::HURT_BOX ||
        collisionRole == CollisionRole::JUST_DODGE_BOX)
    {
        return;
    }

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
    OnPhysicsShapePrepared();
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
    scale_.x *= shapeSizeScale;
    scale_.y *= shapeSizeScale;
    scale_.z *= shapeSizeScale;

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

			// node.globalTransformにはModelのLocalScaleが含まれる。
			// GameObject側のワールドスケールとCollider固有倍率はPhysXのMeshScaleで適用する。
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

void ColliderComponent::SyncFromPhysics(float dt_)
{
    if (characterController && physicsControlsTransform)
    {
        auto owner_ = GetGameObject();
        if (!owner_ || !owner_->transform) return;

        if (HasExternalTransformChange())
        {
            characterController->setPosition(
                PxExtendedVec3(
                    owner_->transform->localPosition.x + offset.x,
                    owner_->transform->localPosition.y + offset.y,
                    owner_->transform->localPosition.z + offset.z));
            CaptureTransformSnapshot();
            return;
        }

        const PxExtendedVec3 position_ = characterController->getPosition();
        owner_->transform->localPosition =
        {
            static_cast<float>(position_.x) - offset.x,
            static_cast<float>(position_.y) - offset.y,
            static_cast<float>(position_.z) - offset.z
        };

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
                    10);
            }
            else if (velocityRotationMode == VelocityRotationMode::FULL_3D)
            {
                owner_->transform->SetForward({ velocity.x, velocity.y, velocity.z });
            }
        }

        owner_->transform->MarkDirty();
        CaptureTransformSnapshot();
        return;
    }

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

void ColliderComponent::PreparePhysicsStep(float deltaTime_)
{
    if (collisionRole == CollisionRole::HIT_BOX ||
        collisionRole == CollisionRole::HURT_BOX ||
        collisionRole == CollisionRole::JUST_DODGE_BOX)
    {
        return;
    }

    if (UsesCharacterController())
    {
        if (!characterController)
        {
            RecreatePhysXActor();
        }
        if (!characterController) return;

        auto owner_ = GetGameObject();
        if (!owner_ || !owner_->transform) return;

        if (auto controllerActor_ = characterController->getActor())
        {
            controllerActor_->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
        }
        if (!enabled) return;

        if (HasExternalTransformChange())
        {
            characterController->setPosition(
                PxExtendedVec3(
                    owner_->transform->localPosition.x + offset.x,
                    owner_->transform->localPosition.y + offset.y,
                    owner_->transform->localPosition.z + offset.z));
            CaptureTransformSnapshot();
        }

        const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
        if (useGravity && !characterGroundedForStep)
        {
            velocity.y += -9.8f * gravityScale * safeDeltaTime_;
        }
        else if (!useGravity)
        {
            velocity.y = 0.0f;
        }

        float verticalDisplacement_ = velocity.y * safeDeltaTime_;
        if (characterGroundedForStep && velocity.y <= 0.0f)
        {
            if (const auto characterCollider_ = dynamic_cast<CharacterCollider*>(this))
            {
                // 接地中は小さな下り段差へ追従させ、重力加速待ちによる浮遊感を防ぐ。
                verticalDisplacement_ -= characterCollider_->GetGroundSnapDistance();
            }
        }

        const PxExtendedVec3 previousPosition_ = characterController->getPosition();
        const PxVec3 displacement_(
            velocity.x * safeDeltaTime_,
            verticalDisplacement_,
            velocity.z * safeDeltaTime_);
        PxControllerFilters filters_(
            nullptr,
            PhysXSystem::GetCharacterControllerQueryFilterCallback(),
            nullptr);
        const PxControllerCollisionFlags collisionFlags_ =
            characterController->move(displacement_, 0.001f, safeDeltaTime_, filters_);

        isGrounded = collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN);
        characterGroundedForStep = isGrounded;
        if ((isGrounded && velocity.y < 0.0f) ||
            (collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_UP) && velocity.y > 0.0f))
        {
            velocity.y = 0.0f;
        }

        PxExtendedVec3 position_ = characterController->getPosition();
        const bool hasSideCollision_ =
            collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_SIDES);
        const bool hasHorizontalMove_ =
            displacement_.x * displacement_.x + displacement_.z * displacement_.z > 0.000001f;
        if (hasSideCollision_ && hasHorizontalMove_ && verticalDisplacement_ <= 0.0f)
        {
            if (const auto characterCollider_ = dynamic_cast<CharacterCollider*>(this);
                characterCollider_ && characterCollider_->ShouldLimitHorizontalLift())
            {
                const double maxY_ =
                    previousPosition_.y + static_cast<double>(characterCollider_->GetMaxHorizontalLift());
                if (position_.y > maxY_)
                {
                    position_.y = maxY_;
                    characterController->setPosition(position_);
                    velocity.y = (std::min)(velocity.y, 0.0f);
                }
            }
        }

        owner_->transform->localPosition =
        {
            static_cast<float>(position_.x) - offset.x,
            static_cast<float>(position_.y) - offset.y,
            static_cast<float>(position_.z) - offset.z
        };
        owner_->transform->MarkDirty();
        CaptureTransformSnapshot();
        return;
    }

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

    OnPhysicsShapePrepared();
}

void ColliderComponent::ApplyDynamicActorSettings(PxRigidDynamic& actor_)
{
    actor_.setRigidBodyFlag(
        PxRigidBodyFlag::eENABLE_CCD,
        collisionRole == CollisionRole::CHARACTER);

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
    case CollisionRole::JUST_DODGE_BOX:
        usePhysicsResponse = false;
        isTrigger = true;
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

    const char* roleNames_[] =
    {
        "Physical",
        "Character",
        "HitBox",
        "HurtBox",
        "Sensor",
        "JustDodgeBox"
    };
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
    ImGui::TextDisabled(
        "Physical/Character: body collision. HitBox/HurtBox/Sensor/JustDodgeBox: detection only.");


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
    if (ImGui::DragFloat("Shape Size Scale", &shapeSizeScale, 0.01f, 0.01f, 100.0f))
    {
        UpdatePhysXShape();
    }
    ImGui::ColorEdit4("PrimitiveColor", &debugPrimitiveColor.x);

    if (previousStatic_ != isStatic || previousTrigger_ != isTrigger
        || previousPhysicsControlsTransform_ != physicsControlsTransform)
    {
        RecreatePhysXActor();
    }
}


// ColliderComponent.h 蜀・
void ColliderComponent::Serialize(nlohmann::json& j) const  {
    j["Offset"] = { offset.x, offset.y, offset.z };
    j["shapeSizeScale"] = shapeSizeScale;
    j["IsTrigger"] = isTrigger;
    j["IsStatic"] = isStatic;
    j["UsePhysicsResponse"] = usePhysicsResponse;
    j["PhysicsControlsTransform"] = physicsControlsTransform;
    j["CollisionRole"] = static_cast<int>(collisionRole);
    j["ShapeRenderFlg"] = shapeRenderEnabled;
    j["UseGravity"] = useGravity;
    j["GravityScale"] = gravityScale;
    j["VelocityRotationMode"] = static_cast<int>(velocityRotationMode);
    j["RotateVelocityThreshold"] = rotateVelocityThreshold;
    j["ContainmentMode"] = static_cast<int>(containmentMode);
    j["Mass"] = mass;
    j["DebugPrimitiveColor"] = { debugPrimitiveColor.x,debugPrimitiveColor.y,debugPrimitiveColor.z,debugPrimitiveColor.w };
}

void ColliderComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("Offset")) {
        offset.x = j["Offset"][0];
        offset.y = j["Offset"][1];
        offset.z = j["Offset"][2];
    }
    shapeSizeScale = j.value("shapeSizeScale", 1.0f);
    isTrigger = j.value("IsTrigger", false);
    isStatic = j.value("IsStatic", false);
    usePhysicsResponse = j.value("UsePhysicsResponse", true);
    physicsControlsTransform = j.value("PhysicsControlsTransform", false);
    collisionRole = static_cast<CollisionRole>(j.value("CollisionRole", static_cast<int>(CollisionRole::PHYSICAL)));
    shapeRenderEnabled = j.value("ShapeRenderFlg", true);
    useGravity = j.value("UseGravity", false);
    gravityScale = j.value("GravityScale", 1.0f);
    mass = j.value("Mass", 1.0f);

    velocityRotationMode = static_cast<VelocityRotationMode>(
        j.value(
            "VelocityRotationMode",
            static_cast<int>(VelocityRotationMode::NONE)
        )
        );

    containmentMode = static_cast<CollisionContainmentMode>(
        j.value("ContainmentMode", static_cast<int>(CollisionContainmentMode::NONE))
        );

    rotateVelocityThreshold =
        j.value("RotateVelocityThreshold", 0.001f);

    if (j.contains("DebugPrimitiveColor")) {
        debugPrimitiveColor.x = j["DebugPrimitiveColor"][0];
        debugPrimitiveColor.y = j["DebugPrimitiveColor"][1];
        debugPrimitiveColor.z = j["DebugPrimitiveColor"][2];
        debugPrimitiveColor.w = j["DebugPrimitiveColor"][3];
    }

}
