#include "PrecompiledHeader.h"

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

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include <algorithm>
#include <cmath>

using namespace physx;

namespace
{
    std::shared_ptr<GameObject> ResolveHierarchyCollisionOwnerObject(
        const std::shared_ptr<GameObject>& object_)
    {
        std::shared_ptr<GameObject> collisionOwnerObject_ = object_;
        std::shared_ptr<GameObject> currentObject_ = object_;
        while (currentObject_ && currentObject_->transform)
        {
            const std::shared_ptr<Transform> parentTransform_ =
                currentObject_->transform->parent.lock();
            if (!parentTransform_) break;

            const std::shared_ptr<GameObject> parentObject_ =
                parentTransform_->gameObject.lock();
            if (!parentObject_) break;

            collisionOwnerObject_ = parentObject_;
            currentObject_ = parentObject_;
        }

        return collisionOwnerObject_;
    }

    ColliderComponent* GetColliderFromShape(const PxShape* shape_)
    {
        if (!shape_) return nullptr;

        const PxRigidActor* actor_ = shape_->getActor();
        if (!actor_) return nullptr;

        return static_cast<ColliderComponent*>(actor_->userData);
    }

    bool HasSameCollisionOwner(ColliderComponent* colliderA_, ColliderComponent* colliderB_)
    {
        if (!colliderA_ || !colliderB_) return false;

        const uint32_t ownerIdA_ = colliderA_->GetCollisionOwnerId();
        const uint32_t ownerIdB_ = colliderB_->GetCollisionOwnerId();
        return ownerIdA_ != 0u && ownerIdB_ != 0u && ownerIdA_ == ownerIdB_;
    }

    float Clamp01(float value_)
    {
        return (std::max)(0.0f, (std::min)(value_, 1.0f));
    }

    bool IsFiniteFloat(float value_)
    {
        return std::isfinite(value_);
    }

    bool IsFiniteFloat3(const DirectX::XMFLOAT3& value_)
    {
        return IsFiniteFloat(value_.x) &&
            IsFiniteFloat(value_.y) &&
            IsFiniteFloat(value_.z);
    }

    DirectX::XMFLOAT3 SanitizeScale(const DirectX::XMFLOAT3& scale_)
    {
        constexpr float MIN_SCALE = 0.001f;
        return
        {
            IsFiniteFloat(scale_.x) ? (std::max)(std::abs(scale_.x), MIN_SCALE) : MIN_SCALE,
            IsFiniteFloat(scale_.y) ? (std::max)(std::abs(scale_.y), MIN_SCALE) : MIN_SCALE,
            IsFiniteFloat(scale_.z) ? (std::max)(std::abs(scale_.z), MIN_SCALE) : MIN_SCALE
        };
    }

    float SanitizePositiveSize(float value_)
    {
        constexpr float MIN_SIZE = 0.001f;
        if (!IsFiniteFloat(value_)) return MIN_SIZE;
        return (std::max)(std::abs(value_), MIN_SIZE);
    }

    PxVec3 ToPxVec3(const DirectX::XMFLOAT3& value_)
    {
        return PxVec3(value_.x, value_.y, value_.z);
    }

    bool IsFinitePxVec3(const PxVec3& value_)
    {
        return IsFiniteFloat(value_.x) &&
            IsFiniteFloat(value_.y) &&
            IsFiniteFloat(value_.z);
    }

    bool TryBuildActorPose(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& rotation_,
        PxTransform& pose_)
    {
        if (!IsFiniteFloat3(position_) || !IsFiniteFloat3(rotation_)) return false;

        pose_ = PxTransform(
            PxVec3(position_.x, position_.y, position_.z),
            EulerToPxQuat(rotation_));
        return pose_.isSane();
    }

    void ClosestPointsOnSegments(
        const DirectX::XMVECTOR& startA_,
        const DirectX::XMVECTOR& endA_,
        const DirectX::XMVECTOR& startB_,
        const DirectX::XMVECTOR& endB_,
        DirectX::XMVECTOR& closestA_,
        DirectX::XMVECTOR& closestB_)
    {
        using namespace DirectX;

        const XMVECTOR segmentA_ = XMVectorSubtract(endA_, startA_);
        const XMVECTOR segmentB_ = XMVectorSubtract(endB_, startB_);
        const XMVECTOR startDelta_ = XMVectorSubtract(startA_, startB_);

        const float a_ = XMVectorGetX(XMVector3Dot(segmentA_, segmentA_));
        const float b_ = XMVectorGetX(XMVector3Dot(segmentA_, segmentB_));
        const float c_ = XMVectorGetX(XMVector3Dot(segmentB_, segmentB_));
        const float d_ = XMVectorGetX(XMVector3Dot(segmentA_, startDelta_));
        const float e_ = XMVectorGetX(XMVector3Dot(segmentB_, startDelta_));
        const float denominator_ = a_ * c_ - b_ * b_;

        float parameterA_ = 0.0f;
        float parameterB_ = 0.0f;

        if (a_ <= 0.000001f && c_ <= 0.000001f)
        {
            closestA_ = startA_;
            closestB_ = startB_;
            return;
        }

        if (a_ <= 0.000001f)
        {
            parameterB_ = Clamp01(e_ / c_);
        }
        else if (c_ <= 0.000001f)
        {
            parameterA_ = Clamp01(-d_ / a_);
        }
        else
        {
            if (denominator_ != 0.0f)
            {
                parameterA_ = Clamp01((b_ * e_ - c_ * d_) / denominator_);
            }

            parameterB_ = (b_ * parameterA_ + e_) / c_;
            if (parameterB_ < 0.0f)
            {
                parameterB_ = 0.0f;
                parameterA_ = Clamp01(-d_ / a_);
            }
            else if (parameterB_ > 1.0f)
            {
                parameterB_ = 1.0f;
                parameterA_ = Clamp01((b_ - d_) / a_);
            }
        }

        closestA_ = XMVectorAdd(startA_, XMVectorScale(segmentA_, parameterA_));
        closestB_ = XMVectorAdd(startB_, XMVectorScale(segmentB_, parameterB_));
    }

    DirectX::XMFLOAT3 ToFloat3(const DirectX::XMVECTOR& vector_)
    {
        DirectX::XMFLOAT3 value_{};
        DirectX::XMStoreFloat3(&value_, vector_);
        return value_;
    }

    PxVec3 BuildHorizontalPushDirection(
        const DirectX::XMFLOAT3& selfPoint_,
        const DirectX::XMFLOAT3& otherPoint_,
        const DirectX::XMFLOAT3& selfCenter_,
        const PxVec3& fallbackDirection_)
    {
        PxVec3 direction_(
            selfPoint_.x - otherPoint_.x,
            0.0f,
            selfPoint_.z - otherPoint_.z);
        if (direction_.magnitudeSquared() > 0.000001f)
        {
            return direction_.getNormalized();
        }

        direction_ = PxVec3(
            selfCenter_.x - otherPoint_.x,
            0.0f,
            selfCenter_.z - otherPoint_.z);
        if (direction_.magnitudeSquared() > 0.000001f)
        {
            return direction_.getNormalized();
        }

        direction_ = PxVec3(fallbackDirection_.x, 0.0f, fallbackDirection_.z);
        if (direction_.magnitudeSquared() > 0.000001f)
        {
            return direction_.getNormalized();
        }

        return PxVec3(0.0f, 0.0f, 0.0f);
    }

    bool TryBuildCapsuleOverlapPush(
        const PxExtendedVec3& position_,
        const PxCapsuleGeometry& geometry_,
        CapsuleCollider& otherCapsule_,
        const PxVec3& fallbackDirection_,
        PxVec3& displacement_)
    {
        using namespace DirectX;

        const XMVECTOR selfCenter_ = XMVectorSet(
            static_cast<float>(position_.x),
            static_cast<float>(position_.y),
            static_cast<float>(position_.z),
            0.0f);
        const XMVECTOR selfAxis_ = XMVectorSet(0.0f, geometry_.halfHeight, 0.0f, 0.0f);
        const Capsule selfCapsule_
        {
            XMVectorAdd(selfCenter_, selfAxis_),
            XMVectorSubtract(selfCenter_, selfAxis_),
            geometry_.radius
        };
        const Capsule otherCapsuleData_ = otherCapsule_.GetCapsuleData();

        XMVECTOR closestSelf_{};
        XMVECTOR closestOther_{};
        ClosestPointsOnSegments(
            selfCapsule_.p0,
            selfCapsule_.p1,
            otherCapsuleData_.p0,
            otherCapsuleData_.p1,
            closestSelf_,
            closestOther_);

        const XMFLOAT3 closestSelfFloat_ = ToFloat3(closestSelf_);
        const XMFLOAT3 closestOtherFloat_ = ToFloat3(closestOther_);
        const XMFLOAT3 selfCenterFloat_ = ToFloat3(selfCenter_);
        const float horizontalDistance_ = std::sqrt(
            (closestSelfFloat_.x - closestOtherFloat_.x) *
            (closestSelfFloat_.x - closestOtherFloat_.x) +
            (closestSelfFloat_.z - closestOtherFloat_.z) *
            (closestSelfFloat_.z - closestOtherFloat_.z));
        const float radiusSum_ = selfCapsule_.radius + otherCapsuleData_.radius;
        const float horizontalDepth_ = radiusSum_ - horizontalDistance_;
        if (horizontalDepth_ <= 0.0f) return false;

        const PxVec3 horizontalDirection_ = BuildHorizontalPushDirection(
            closestSelfFloat_,
            closestOtherFloat_,
            selfCenterFloat_,
            fallbackDirection_);
        if (horizontalDirection_.magnitudeSquared() <= 0.000001f) return false;

        displacement_ = horizontalDirection_ * horizontalDepth_;
        return true;
    }

    bool TryGetNonGroundPush(
        const PxExtendedVec3& position_,
        const PxCapsuleGeometry& geometry_,
        ColliderComponent* selfCollider_,
        float maxHorizontalDistance_,
        float maxLiftDistance_,
        bool forceLift_,
        PxVec3& pushDisplacement_)
    {
        BS_PROFILE_SCOPE("ColliderComponent::TryGetNonGroundPush");

        PxScene* scene_ = PhysXSystem::GetScene();
        if (!scene_ || !selfCollider_) return false;

        const PxTransform selfPose_(
            PxVec3(
                static_cast<PxReal>(position_.x),
                static_cast<PxReal>(position_.y),
                static_cast<PxReal>(position_.z)),
            PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f)));
        PxOverlapHit overlapHits_[16];
        PxOverlapBuffer overlapBuffer_(overlapHits_, 16);
        const PxQueryFilterData queryFilterData_(
            PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);

        if (!scene_->overlap(
            geometry_,
            selfPose_,
            overlapBuffer_,
            queryFilterData_))
        {
            return false;
        }

        PxVec3 accumulatedDisplacement_(0.0f, 0.0f, 0.0f);
        for (PxU32 hitIndex_ = 0; hitIndex_ < overlapBuffer_.getNbAnyHits(); ++hitIndex_)
        {
            const PxOverlapHit& hit_ = overlapBuffer_.getAnyHit(hitIndex_);
            ColliderComponent* otherCollider_ = GetColliderFromShape(hit_.shape);
            if (!otherCollider_ || otherCollider_ == selfCollider_) continue;
            if (HasSameCollisionOwner(selfCollider_, otherCollider_)) continue;
            if (otherCollider_->CanBeCharacterGround()) continue;

            const PxRigidActor* otherActor_ = hit_.actor;
            if (!otherActor_) continue;

            PxVec3 direction_(0.0f, 0.0f, 0.0f);
            PxF32 depth_ = 0.0f;
            const PxGeometryHolder otherGeometry_ = hit_.shape->getGeometry();
            const PxTransform otherPose_ =
                otherActor_->getGlobalPose() * hit_.shape->getLocalPose();
            if (!PxGeometryQuery::computePenetration(
                direction_,
                depth_,
                geometry_,
                selfPose_,
                otherGeometry_.any(),
                otherPose_))
            {
                continue;
            }

            if (direction_.magnitudeSquared() <= 0.000001f || depth_ <= 0.0f) continue;

            PxVec3 displacement_ = direction_.getNormalized() * depth_;
            if (CapsuleCollider* otherCapsule_ =
                dynamic_cast<CapsuleCollider*>(otherCollider_))
            {
                PxVec3 capsuleDisplacement_(0.0f, 0.0f, 0.0f);
                if (TryBuildCapsuleOverlapPush(
                    position_,
                    geometry_,
                    *otherCapsule_,
                    direction_,
                    capsuleDisplacement_))
                {
                    displacement_.x = capsuleDisplacement_.x;
                    displacement_.z = capsuleDisplacement_.z;
                    displacement_.y = (std::max)(direction_.y * depth_, 0.0f);
                }
                else
                {
                    displacement_.x = 0.0f;
                    displacement_.z = 0.0f;
                    displacement_.y = (std::max)(direction_.y * depth_, 0.0f);
                }
            }

            accumulatedDisplacement_ += displacement_;
        }

        if (accumulatedDisplacement_.magnitudeSquared() <= 0.000001f) return false;

        const PxVec3 horizontalDisplacement_(
            accumulatedDisplacement_.x,
            0.0f,
            accumulatedDisplacement_.z);
        PxVec3 finalDisplacement_(0.0f, 0.0f, 0.0f);
        const float horizontalLength_ = horizontalDisplacement_.magnitude();
        if (horizontalLength_ > 0.000001f && maxHorizontalDistance_ > 0.0f)
        {
            finalDisplacement_ += horizontalDisplacement_.getNormalized() *
                (std::min)(horizontalLength_, maxHorizontalDistance_);
        }

        finalDisplacement_.y = (std::min)(
            (std::max)(accumulatedDisplacement_.y, 0.0f),
            maxLiftDistance_);
        if (forceLift_ && finalDisplacement_.y <= 0.000001f)
        {
            finalDisplacement_.y = maxLiftDistance_;
        }

        if (finalDisplacement_.magnitudeSquared() <= 0.000001f) return false;

        pushDisplacement_ = finalDisplacement_;
        return true;
    }
}

ColliderComponent::~ColliderComponent()
{
    ReleasePhysXActor();
}

void ColliderComponent::Start()
{
    BS_PROFILE_SCOPE("ColliderComponent::Start");

    CreatePhysXActor();
}

void ColliderComponent::ReleasePhysXActor()
{
    BS_PROFILE_SCOPE("ColliderComponent::ReleasePhysXActor");

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
    BS_PROFILE_SCOPE("ColliderComponent::RecreatePhysXActor");

    ReleasePhysXActor();
    CreatePhysXActor();
}

void ColliderComponent::CreateCharacterController()
{
    BS_PROFILE_SCOPE("ColliderComponent::CreateCharacterController");

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
    scale_ = SanitizeScale(scale_);

    auto material_ = GetShapeMaterial();
    if (!material_) return;

    physx::PxCapsuleControllerDesc desc_;
    desc_.material = material_;
    const DirectX::XMFLOAT3 controllerPosition_
    {
        transform_->localPosition.x + offset.x,
        transform_->localPosition.y + offset.y,
        transform_->localPosition.z + offset.z
    };
    if (!IsFiniteFloat3(controllerPosition_)) return;

    desc_.position = physx::PxExtendedVec3(
        controllerPosition_.x,
        controllerPosition_.y,
        controllerPosition_.z
    );

    desc_.radius = SanitizePositiveSize(capsule_->radius * scale_.x);

    // PxCapsuleControllerDesc::height は「円柱部分の高さ」。
    // 既存 CapsuleCollider の halfHeight が PhysX PxCapsuleGeometry の halfHeight と同じ意味なら *2。
    desc_.height = SanitizePositiveSize(capsule_->halfHeight * 2.0f * scale_.y);

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

            const physx::PxU32 ownerId_ = GetCollisionOwnerId();
            filterData_.word3 = ownerId_ | 0x80000000u;

            shape_->setSimulationFilterData(filterData_);
            shape_->setQueryFilterData(filterData_);
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
    BS_PROFILE_SCOPE("ColliderComponent::ReleaseCharacterController");

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

std::shared_ptr<GameObject> ColliderComponent::ResolveCollisionOwnerObject()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    if (!collisionOwnerObjectName.empty() && owner_->manager)
    {
        const std::shared_ptr<GameObject> collisionOwner_ =
            owner_->GetManager().Find(collisionOwnerObjectName);
        if (collisionOwner_) return collisionOwner_;
    }

    return ResolveHierarchyCollisionOwnerObject(owner_);
}

uint32_t ColliderComponent::GetCollisionOwnerId()
{
    const std::shared_ptr<GameObject> collisionOwner_ = ResolveCollisionOwnerObject();
    const GameObject* ownerPointer_ = collisionOwner_
        ? collisionOwner_.get()
        : GetGameObject().get();

    return static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(ownerPointer_) & 0x7FFFFFFF);
}

bool ColliderComponent::UsesCharacterController() const
{
    return collisionRole == CollisionRole::CHARACTER &&
        GetColliderType() == ColliderType::Capsule;
}

void ColliderComponent::CreatePhysXActor()
{
    BS_PROFILE_SCOPE("ColliderComponent::CreatePhysXActor");

    if (UsesCharacterController())
    {
        CreateCharacterController();
        return;
    }

    if (collisionRole == CollisionRole::HIT_BOX ||
        collisionRole == CollisionRole::HURT_BOX ||
        collisionRole == CollisionRole::JUST_DODGE_BOX ||
        collisionRole == CollisionRole::GUARD_BOX)
    {
        return;
    }

    auto owner_ = GetGameObject();
    auto physics_ = PhysXSystem::GetPhysics();
    auto scene_ = PhysXSystem::GetScene();
    if (!owner_ || !physics_ || !scene_) return;

    auto transform_ = owner_->transform;
    PxTransform pose_;
    if (!transform_ || !TryBuildActorPose(transform_->localPosition, transform_->localRotation, pose_))
    {
        return;
    }

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
    BS_PROFILE_SCOPE("ColliderComponent::UpdatePhysXShape");

    if (!actor) return;

    PxShape* shape_ = nullptr;
    if (actor->getNbShapes() > 0)
    {
        actor->getShapes(&shape_, 1);
    }

    auto owner_ = GetGameObject();
    if (!owner_) return;

    if (!owner_->transform) return;

    auto scale_ = owner_->transform->GetWorldScale();
    scale_.x *= shapeSizeScale;
    scale_.y *= shapeSizeScale;
    scale_.z *= shapeSizeScale;
    scale_ = SanitizeScale(scale_);

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
                    SanitizePositiveSize(box_->halfSize.x * scale_.x),
                    SanitizePositiveSize(box_->halfSize.y * scale_.y),
                    SanitizePositiveSize(box_->halfSize.z * scale_.z)),
                *material_);
            break;
        }
        case ColliderType::Sphere:
        {
            auto sphere_ = static_cast<SphereCollider*>(this);
            shape_ = PxRigidActorExt::createExclusiveShape(
                *actor,
                PxSphereGeometry(SanitizePositiveSize(sphere_->radius * scale_.x)),
                *material_);
            break;
        }
        case ColliderType::Capsule:
        {
            auto capsule_ = static_cast<CapsuleCollider*>(this);
            shape_ = PxRigidActorExt::createExclusiveShape(
                *actor,
                PxCapsuleGeometry(
                    SanitizePositiveSize(capsule_->radius * scale_.x),
                    SanitizePositiveSize(capsule_->halfHeight * scale_.y)),
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
                    PxConvexMeshGeometry(mesh_, PxMeshScale(ToPxVec3(scale_))),
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
			const PxMeshScale meshScale_(ToPxVec3(scale_));
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
                SanitizePositiveSize(box_->halfSize.x * scale_.x),
                SanitizePositiveSize(box_->halfSize.y * scale_.y),
                SanitizePositiveSize(box_->halfSize.z * scale_.z)));
            break;
        }
        case ColliderType::Sphere:
        {
            auto sphere_ = static_cast<SphereCollider*>(this);
            shape_->setGeometry(PxSphereGeometry(SanitizePositiveSize(sphere_->radius * scale_.x)));
            break;
        }
        case ColliderType::Capsule:
        {
            auto capsule_ = static_cast<CapsuleCollider*>(this);
            shape_->setGeometry(PxCapsuleGeometry(
                SanitizePositiveSize(capsule_->radius * scale_.x),
                SanitizePositiveSize(capsule_->halfHeight * scale_.y)));
            break;
        }
        case ColliderType::Cylinder:
        case ColliderType::Mesh:
        {
            if (shape_->getGeometry().getType() == PxGeometryType::eCONVEXMESH)
            {
                auto convexGeometry_ = static_cast<const PxConvexMeshGeometry&>(shape_->getGeometry());
                convexGeometry_.scale = PxMeshScale(ToPxVec3(scale_));
                shape_->setGeometry(convexGeometry_);
                break;
            }

            if (shape_->getGeometry().getType() == PxGeometryType::eTRIANGLEMESH)
            {
                auto triangleGeometry_ = static_cast<const PxTriangleMeshGeometry&>(shape_->getGeometry());
                triangleGeometry_.scale = PxMeshScale(ToPxVec3(scale_));
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
    if (!IsFiniteFloat3(offset)) return;
    const PxTransform localPose_(PxVec3(offset.x, offset.y, offset.z), shapeRotation_);
    if (!localPose_.isSane()) return;

    shape_->setLocalPose(localPose_);
    shape_->setFlag(PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
    shape_->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);

    PxFilterData filterData_;
    filterData_.word0 = static_cast<PxU32>(owner_->layer);
    filterData_.word1 = owner_->mask;
    filterData_.word2 = static_cast<PxU32>(collisionRole);
    const PxU32 ownerId_ = GetCollisionOwnerId();
    filterData_.word3 = ownerId_ | (usePhysicsResponse ? 0x80000000u : 0u);
    shape_->setSimulationFilterData(filterData_);
    shape_->setQueryFilterData(filterData_);
}

void ColliderComponent::SyncFromPhysics(float dt_)
{
    if (characterController && physicsControlsTransform)
    {
        auto owner_ = GetGameObject();
        if (!owner_ || !owner_->transform) return;

        if (HasExternalTransformChange())
        {
            const DirectX::XMFLOAT3 controllerPosition_
            {
                owner_->transform->localPosition.x + offset.x,
                owner_->transform->localPosition.y + offset.y,
                owner_->transform->localPosition.z + offset.z
            };
            if (!IsFiniteFloat3(controllerPosition_)) return;

            characterController->setPosition(
                PxExtendedVec3(
                    controllerPosition_.x,
                    controllerPosition_.y,
                    controllerPosition_.z));
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
    horizontalMovementBlocked = false;
    BS_PROFILE_SCOPE("ColliderComponent::PreparePhysicsStep");

    if (collisionRole == CollisionRole::HIT_BOX ||
        collisionRole == CollisionRole::HURT_BOX ||
        collisionRole == CollisionRole::JUST_DODGE_BOX ||
        collisionRole == CollisionRole::GUARD_BOX)
    {
        return;
    }

    if (UsesCharacterController())
    {
        BS_PROFILE_SCOPE("ColliderComponent::PreparePhysicsStep::CharacterController");

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
            const DirectX::XMFLOAT3 controllerPosition_
            {
                owner_->transform->localPosition.x + offset.x,
                owner_->transform->localPosition.y + offset.y,
                owner_->transform->localPosition.z + offset.z
            };
            if (!IsFiniteFloat3(controllerPosition_)) return;

            characterController->setPosition(
                PxExtendedVec3(
                    controllerPosition_.x,
                    controllerPosition_.y,
                    controllerPosition_.z));
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

        const PxExtendedVec3 previousPosition_ = characterController->getPosition();
        const auto characterCollider_ = dynamic_cast<CharacterCollider*>(this);
        PxCapsuleGeometry characterGeometry_(0.001f, 0.001f);
        if (characterCollider_)
        {
            const std::shared_ptr<GameObject> ownerObject_ = GetGameObject();
            auto scale_ = ownerObject_ && ownerObject_->transform
                ? ownerObject_->transform->GetWorldScale()
                : DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f };
            scale_.x *= shapeSizeScale;
            scale_.y *= shapeSizeScale;
            scale_.z *= shapeSizeScale;
            scale_ = SanitizeScale(scale_);
            characterGeometry_ = PxCapsuleGeometry(
                SanitizePositiveSize(characterCollider_->radius * scale_.x),
                SanitizePositiveSize(characterCollider_->halfHeight * scale_.y));
        }

        PxVec3 preNonGroundPushDisplacement_(0.0f, 0.0f, 0.0f);
        const bool hasPreNonGroundSupport_ =
            characterCollider_ &&
            characterCollider_->ShouldSlideOffNonGround() &&
            TryGetNonGroundPush(
                previousPosition_,
                characterGeometry_,
                this,
                characterCollider_->GetNonGroundSlideSpeed() * safeDeltaTime_,
                characterCollider_->GetNonGroundLift(),
                true,
                preNonGroundPushDisplacement_);

        bool hasNonGroundVelocity_ = false;
        if (hasPreNonGroundSupport_ && safeDeltaTime_ > 0.0f)
        {
            const float inverseDeltaTime_ = 1.0f / safeDeltaTime_;
            velocity.x += preNonGroundPushDisplacement_.x * inverseDeltaTime_;
            velocity.z += preNonGroundPushDisplacement_.z * inverseDeltaTime_;
            velocity.y = (std::max)(
                velocity.y,
                preNonGroundPushDisplacement_.y * inverseDeltaTime_);
            hasNonGroundVelocity_ = true;
        }

        float verticalDisplacement_ = velocity.y * safeDeltaTime_;
        if (characterGroundedForStep && !hasPreNonGroundSupport_ && velocity.y <= 0.0f)
        {
            if (characterCollider_)
            {
                verticalDisplacement_ -= characterCollider_->GetGroundSnapDistance();
            }
        }

        const PxVec3 displacement_(
            velocity.x * safeDeltaTime_,
            verticalDisplacement_,
            velocity.z * safeDeltaTime_);
        if (!IsFinitePxVec3(displacement_))
        {
            velocity = { 0.0f, 0.0f, 0.0f };
            return;
        }

        PxFilterData filterData_;
        filterData_.word0 = static_cast<PxU32>(owner_->layer);
        filterData_.word1 = owner_->mask;
        filterData_.word2 = static_cast<PxU32>(collisionRole);
        filterData_.word3 = GetCollisionOwnerId() | 0x80000000u;
        PxControllerFilters filters_(
            &filterData_,
            PhysXSystem::GetCharacterControllerQueryFilterCallback(),
            PhysXSystem::GetCharacterControllerFilterCallback());
        const PxControllerCollisionFlags collisionFlags_ =
            characterController->move(displacement_, 0.001f, safeDeltaTime_, filters_);

        PxExtendedVec3 position_ = characterController->getPosition();
        PxVec3 nonGroundPushCheckDisplacement_(0.0f, 0.0f, 0.0f);
        const bool hasNonGroundSupport_ =
            (hasNonGroundVelocity_ ||
                collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN)) &&
            characterCollider_ &&
            characterCollider_->ShouldSlideOffNonGround() &&
            (hasNonGroundVelocity_ ||
                TryGetNonGroundPush(
                    position_,
                    characterGeometry_,
                    this,
                    characterCollider_->GetNonGroundSlideSpeed() * safeDeltaTime_,
                    characterCollider_->GetNonGroundLift(),
                    false,
                    nonGroundPushCheckDisplacement_));

        isGrounded =
            collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN) &&
            !hasNonGroundSupport_;
        characterGroundedForStep = isGrounded;
        if ((isGrounded && velocity.y < 0.0f) ||
            (collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_UP) && velocity.y > 0.0f))
        {
            velocity.y = 0.0f;
        }

        position_ = characterController->getPosition();
        const bool hasSideCollision_ =
            collisionFlags_.isSet(PxControllerCollisionFlag::eCOLLISION_SIDES);
        horizontalMovementBlocked = hasSideCollision_;
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
        BS_PROFILE_SCOPE("ColliderComponent::PreparePhysicsStep::RecreateActor");

        RecreatePhysXActor();
    }
    if (!actor) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
    if (!enabled) return;

    UpdatePhysXShape();

    auto transform_ = owner_->transform;
    PxTransform pose_;
    if (!transform_ || !TryBuildActorPose(transform_->localPosition, transform_->localRotation, pose_))
    {
        return;
    }
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
    BS_PROFILE_SCOPE("ColliderComponent::PushTransformToPhysics");

    if (!actor) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    const auto transform_ = owner_->transform;
    PxTransform pose_;
    if (!transform_ || !TryBuildActorPose(transform_->localPosition, transform_->localRotation, pose_))
    {
        return;
    }
    actor->setGlobalPose(pose_);
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
    case CollisionRole::GUARD_BOX:
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

    char colliderNameBuffer_[128];
    strncpy_s(colliderNameBuffer_, colliderName.c_str(), sizeof(colliderNameBuffer_));
    if (ImGui::InputText("Collider Name", colliderNameBuffer_, sizeof(colliderNameBuffer_)))
    {
        colliderName = colliderNameBuffer_;
    }

    const char* roleNames_[] =
    {
        "Physical",
        "Character",
        "HitBox",
        "HurtBox",
        "Sensor",
        "JustDodgeBox",
        "GuardBox"
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
    ImGui::Checkbox("Character Ground", &canBeCharacterGround);
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
    j["ColliderName"] = colliderName;
    j["IsTrigger"] = isTrigger;
    j["IsStatic"] = isStatic;
    j["UsePhysicsResponse"] = usePhysicsResponse;
    j["PhysicsControlsTransform"] = physicsControlsTransform;
    j["CollisionRole"] = static_cast<int>(collisionRole);
    j["CollisionOwnerObjectName"] = collisionOwnerObjectName;
    j["ShapeRenderFlg"] = shapeRenderEnabled;
    j["CanBeCharacterGround"] = canBeCharacterGround;
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
    colliderName = j.value("ColliderName", std::string{});
    collisionOwnerObjectName = j.value("CollisionOwnerObjectName", std::string{});
    shapeRenderEnabled = j.value("ShapeRenderFlg", true);
    canBeCharacterGround = j.value("CanBeCharacterGround", true);
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
