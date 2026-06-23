#include "CameraCollisionComponent.h"

#include "CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/PhysXSystem.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace physx;

namespace
{
    class CameraQueryFilter : public PxQueryFilterCallback
    {
    public:
        explicit CameraQueryFilter(const std::vector<const void*>& ignoredUserData_)
            : ignoredUserData(ignoredUserData_)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData_,
            const PxShape* shape_,
            const PxRigidActor* actor_,
            PxHitFlags& queryFlags_) override
        {
            (void)filterData_;
            (void)queryFlags_;
            if (!shape_ || !actor_) return PxQueryHitType::eNONE;
            if (!shape_->getFlags().isSet(PxShapeFlag::eSCENE_QUERY_SHAPE))
            {
                return PxQueryHitType::eNONE;
            }
            if (shape_->getFlags().isSet(PxShapeFlag::eTRIGGER_SHAPE))
            {
                return PxQueryHitType::eNONE;
            }
            if (std::find(
                ignoredUserData.begin(),
                ignoredUserData.end(),
                actor_->userData) != ignoredUserData.end())
            {
                return PxQueryHitType::eNONE;
            }
            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum postFilter(
            const PxFilterData& filterData_,
            const PxQueryHit& hit_,
            const PxShape* shape_,
            const PxRigidActor* actor_) override
        {
            (void)filterData_;
            (void)hit_;
            (void)shape_;
            (void)actor_;
            return PxQueryHitType::eBLOCK;
        }

    private:
        const std::vector<const void*>& ignoredUserData;
    };

    void CollectIgnoredUserData(
        const std::shared_ptr<GameObject>& object_,
        std::vector<const void*>& userData_)
    {
        if (!object_) return;
        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            if (collider_) userData_.push_back(collider_.get());
        }
    }
}

void CameraCollisionComponent::Start()
{
    ResolveReferences();
}

void CameraCollisionComponent::LateUpdate(float deltaTime_)
{
    (void)deltaTime_;
    ResolveReferences();

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    PxScene* scene_ = PhysXSystem::GetScene();
    if (!camera_ || !scene_) return;

    const DirectX::XMFLOAT3 start_ = camera_->GetFocus();
    const DirectX::XMFLOAT3 desiredPosition_ = camera_->GetEye();
    const DirectX::XMFLOAT3 difference_
    {
        desiredPosition_.x - start_.x,
        desiredPosition_.y - start_.y,
        desiredPosition_.z - start_.z
    };
    const float desiredDistance_ = std::sqrt(
        difference_.x * difference_.x +
        difference_.y * difference_.y +
        difference_.z * difference_.z);
    if (desiredDistance_ <= 0.001f) return;

    const PxVec3 origin_(start_.x, start_.y, start_.z);
    const PxVec3 direction_(
        difference_.x / desiredDistance_,
        difference_.y / desiredDistance_,
        difference_.z / desiredDistance_);

    std::vector<const void*> ignoredUserData_;
    CollectIgnoredUserData(GetGameObject(), ignoredUserData_);
    CollectIgnoredUserData(ignoreTarget.lock(), ignoredUserData_);
    CollectIgnoredUserData(ignoreLockOnTarget.lock(), ignoredUserData_);

    CameraQueryFilter queryFilter_(ignoredUserData_);

    PxQueryFlags queryFlags_ = PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;
    if (includeDynamicColliders) queryFlags_ |= PxQueryFlag::eDYNAMIC;
    const PxQueryFilterData filterData_(PxFilterData(), queryFlags_);
    const PxHitFlags hitFlags_ =
        PxHitFlag::eDEFAULT | PxHitFlag::eMESH_BOTH_SIDES;
    float hitDistance_ = desiredDistance_;
    bool hasHit_ = false;
    if (probeRadius > 0.001f)
    {
        PxSweepBuffer sweepHit_;
        const PxSphereGeometry probeGeometry_((std::max)(probeRadius, 0.001f));
        hasHit_ = scene_->sweep(
            probeGeometry_,
            PxTransform(origin_),
            direction_,
            desiredDistance_,
            sweepHit_,
            hitFlags_,
            filterData_,
            &queryFilter_);
        if (hasHit_) hitDistance_ = sweepHit_.block.distance;
    }
    else
    {
        PxRaycastBuffer rayHit_;
        hasHit_ = scene_->raycast(
            origin_,
            direction_,
            desiredDistance_,
            rayHit_,
            hitFlags_,
            filterData_,
            &queryFilter_);
        if (hasHit_) hitDistance_ = rayHit_.block.distance;
    }
    if (!hasHit_)
    {
        return;
    }

    if (!std::isfinite(hitDistance_)) return;

    const float safeMinimumDistance_ = std::clamp(
        minimumCameraDistance,
        0.001f,
        desiredDistance_);
    const float allowedDistance_ = std::clamp(
        hitDistance_ - (std::max)(collisionOffset, 0.0f),
        safeMinimumDistance_,
        desiredDistance_);
    const DirectX::XMFLOAT3 limitedPosition_
    {
        start_.x + direction_.x * allowedDistance_,
        start_.y + direction_.y * allowedDistance_,
        start_.z + direction_.z * allowedDistance_
    };
    ApplyCameraPose(limitedPosition_, start_);
}

void CameraCollisionComponent::ResolveReferences()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (camera.expired())
    {
        camera = owner_->GetComponent<CameraComponent>();
    }

    if (ignoreTarget.expired() && owner_->manager && !ignoreTargetName.empty())
    {
        ignoreTarget = owner_->manager->Find(ignoreTargetName);
    }

    if (ignoreLockOnTarget.expired() && owner_->manager && !ignoreLockOnTargetName.empty())
    {
        ignoreLockOnTarget = owner_->manager->Find(ignoreLockOnTargetName);
    }
}

void CameraCollisionComponent::ApplyCameraPose(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&cameraPosition_),
        DirectX::XMLoadFloat3(&lookAtPosition_),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    DirectX::XMVECTOR determinant_;
    owner_->transform->ApplyWorldMatrix(
        DirectX::XMMatrixInverse(&determinant_, viewMatrix_));
    if (const std::shared_ptr<CameraComponent> camera_ = camera.lock())
    {
        camera_->SetLookAt(
            cameraPosition_,
            lookAtPosition_,
            { 0.0f, 1.0f, 0.0f });
    }
}

void CameraCollisionComponent::Gui()
{
    char targetBuffer_[128]{};
    strcpy_s(targetBuffer_, ignoreTargetName.c_str());
    if (ImGui::InputText("Ignore Target", targetBuffer_, sizeof(targetBuffer_)))
    {
        ignoreTargetName = targetBuffer_;
        ignoreTarget.reset();
        ResolveReferences();
    }
    char lockOnTargetBuffer_[128]{};
    strcpy_s(lockOnTargetBuffer_, ignoreLockOnTargetName.c_str());
    if (ImGui::InputText("Ignore LockOn Target", lockOnTargetBuffer_, sizeof(lockOnTargetBuffer_)))
    {
        ignoreLockOnTargetName = lockOnTargetBuffer_;
        ignoreLockOnTarget.reset();
        ResolveReferences();
    }

    ImGui::DragFloat("Collision Offset", &collisionOffset, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat(
        "Minimum Camera Distance",
        &minimumCameraDistance,
        0.01f,
        0.001f,
        5.0f);
    ImGui::DragFloat("Probe Radius", &probeRadius, 0.01f, 0.0f, 5.0f);
    ImGui::Checkbox("Include Dynamic Colliders", &includeDynamicColliders);
}

void CameraCollisionComponent::Serialize(nlohmann::json& json_) const
{
    json_["ignoreTargetName"] = ignoreTargetName;
    json_["ignoreLockOnTargetName"] = ignoreLockOnTargetName;

    json_["collisionOffset"] = collisionOffset;
    json_["minimumCameraDistance"] = minimumCameraDistance;
    json_["probeRadius"] = probeRadius;
    json_["includeDynamicColliders"] = includeDynamicColliders;
}

void CameraCollisionComponent::Deserialize(const nlohmann::json& json_)
{
    ignoreTargetName = json_.value("ignoreTargetName", std::string{});
    ignoreLockOnTargetName = json_.value("ignoreLockOnTargetName", std::string{});

    collisionOffset = json_.value("collisionOffset", 0.15f);
    minimumCameraDistance = json_.value("minimumCameraDistance", 0.05f);
    probeRadius = json_.value("probeRadius", 0.3f);
    includeDynamicColliders = json_.value("includeDynamicColliders", true);
    ignoreTarget.reset();
}

REGISTER_COMPONENT(CameraCollisionComponent, "CameraCollisionComponent")
