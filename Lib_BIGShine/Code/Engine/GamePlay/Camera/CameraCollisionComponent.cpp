#include "PrecompiledHeader.h"

#include "CameraCollisionComponent.h"

#include "CameraComponent.h"
#include "CameraLockOnComponent.h"
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

    float DistanceFloat3(
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_)
    {
        const float differenceX_ = endPosition_.x - startPosition_.x;
        const float differenceY_ = endPosition_.y - startPosition_.y;
        const float differenceZ_ = endPosition_.z - startPosition_.z;
        return std::sqrt(
            differenceX_ * differenceX_ +
            differenceY_ * differenceY_ +
            differenceZ_ * differenceZ_);
    }

}

void CameraCollisionComponent::Start()
{
    ResolveReferences();
}

void CameraCollisionComponent::LateUpdate(float deltaTime_)
{
    ResolveReferences();

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_ || !PhysXSystem::GetScene()) return;

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

    const std::shared_ptr<CameraLockOnComponent> lockOnCamera_ =
        lockOnCamera.lock();
    std::vector<const void*> ignoredUserData_;
    CollectIgnoredUserData(GetGameObject(), ignoredUserData_);
    CollectIgnoredUserData(ignoreTarget.lock(), ignoredUserData_);
    CollectIgnoredUserData(ignoreLockOnTarget.lock(), ignoredUserData_);
    if (lockOnCamera_)
    {
        CollectIgnoredUserData(lockOnCamera_->GetPlayerTarget(), ignoredUserData_);
        CollectIgnoredUserData(lockOnCamera_->GetLockOnTarget(), ignoredUserData_);
    }

    float hitDistance_ = desiredDistance_;
    const bool hasHit_ = SweepCameraSegment(
        start_,
        desiredPosition_,
        ignoredUserData_,
        hitDistance_);
    if (hasHit_ && !std::isfinite(hitDistance_)) return;

    const DirectX::XMFLOAT3 direction_
    {
        difference_.x / desiredDistance_,
        difference_.y / desiredDistance_,
        difference_.z / desiredDistance_
    };

    const float safeMinimumDistance_ = std::clamp(
        minimumCameraDistance,
        0.001f,
        desiredDistance_);
    const float safeHitDistance_ = hasHit_
        ? hitDistance_ - (std::max)(collisionOffset, 0.0f)
        : desiredDistance_;
    const float allowedDistance_ = std::clamp(
        safeHitDistance_,
        safeMinimumDistance_,
        desiredDistance_);
    const DirectX::XMFLOAT3 limitedPosition_
    {
        start_.x + direction_.x * allowedDistance_,
        start_.y + direction_.y * allowedDistance_,
        start_.z + direction_.z * allowedDistance_
    };

    debugDesiredDistance = desiredDistance_;
    debugCorrectedDistance = allowedDistance_;
    debugCompressionRate = std::clamp(
        1.0f - allowedDistance_ / desiredDistance_,
        0.0f,
        1.0f);

    const bool isLockOnActive_ =
        lockOnCamera_ &&
        lockOnCamera_->IsLockOnEnabled();
    const bool canLift_ = enableLockOnCollisionLift && isLockOnActive_;
    // 壁際専用カメラが内側へ逃がした後に、通常の持ち上げ補正で上へ跳ねすぎないよう弱める。
    const float arenaWallCameraRate_ =
        reduceCollisionLiftWhenArenaWallCameraActive &&
            lockOnCamera_ &&
            lockOnCamera_->IsWallCameraActive()
        ? lockOnCamera_->GetWallCameraRate()
        : 0.0f;
    const float safeLiftStartRate_ = std::clamp(
        collisionLiftStartRate,
        0.0f,
        0.95f);
    const float liftRate_ = canLift_
        ? std::clamp(
            (debugCompressionRate - safeLiftStartRate_) /
                (1.0f - safeLiftStartRate_),
            0.0f,
            1.0f)
        : 0.0f;
    const float targetCollisionLift_ =
        (std::max)(maxCollisionLiftHeight, 0.0f) *
        liftRate_ *
        (1.0f - std::clamp(arenaWallCameraRate_, 0.0f, 1.0f));
    const float liftSpeed_ = targetCollisionLift_ > currentCollisionLift
        ? collisionLiftUpSpeed
        : collisionLiftDownSpeed;
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
    const float smoothingRate_ = std::clamp(
        1.0f - std::exp(-(std::max)(liftSpeed_, 0.1f) * safeDeltaTime_),
        0.0f,
        1.0f);
    currentCollisionLift = std::lerp(
        currentCollisionLift,
        targetCollisionLift_,
        smoothingRate_);

    DirectX::XMFLOAT3 finalPosition_ = limitedPosition_;
    if (canLift_ && currentCollisionLift > 0.001f)
    {
        DirectX::XMFLOAT3 liftedPosition_ = limitedPosition_;
        liftedPosition_.y += currentCollisionLift;

        float liftHitDistance_ = currentCollisionLift;
        if (SweepCameraSegment(
            limitedPosition_,
            liftedPosition_,
            ignoredUserData_,
            liftHitDistance_) &&
            std::isfinite(liftHitDistance_))
        {
            const float safeLiftDistance_ = std::clamp(
                liftHitDistance_ - (std::max)(collisionOffset, 0.0f),
                0.0f,
                currentCollisionLift);
            finalPosition_.y += safeLiftDistance_;
        }
        else
        {
            finalPosition_ = liftedPosition_;
        }
    }

    if (!hasHit_ && currentCollisionLift <= 0.001f)
    {
        return;
    }

    if (isLockOnActive_ &&
        lockOnCamera_->ApplyCollisionComposition(
            finalPosition_,
            start_,
            debugCompressionRate,
            currentCollisionLift,
            deltaTime_))
    {
        return;
    }
    ApplyCameraPose(finalPosition_, start_);
}

void CameraCollisionComponent::ResolveReferences()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (camera.expired())
    {
        camera = owner_->GetComponent<CameraComponent>();
    }

    if (lockOnCamera.expired())
    {
        lockOnCamera = owner_->GetComponent<CameraLockOnComponent>();
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

bool CameraCollisionComponent::SweepCameraSegment(
    const DirectX::XMFLOAT3& startPosition_,
    const DirectX::XMFLOAT3& endPosition_,
    const std::vector<const void*>& ignoredUserData_,
    float& hitDistance_) const
{
    PxScene* scene_ = PhysXSystem::GetScene();
    if (!scene_) return false;

    const float distance_ = DistanceFloat3(startPosition_, endPosition_);
    if (distance_ <= 0.001f) return false;

    const PxVec3 origin_(startPosition_.x, startPosition_.y, startPosition_.z);
    const PxVec3 direction_(
        (endPosition_.x - startPosition_.x) / distance_,
        (endPosition_.y - startPosition_.y) / distance_,
        (endPosition_.z - startPosition_.z) / distance_);

    CameraQueryFilter queryFilter_(ignoredUserData_);

    PxQueryFlags queryFlags_ = PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;
    if (includeDynamicColliders) queryFlags_ |= PxQueryFlag::eDYNAMIC;
    const PxQueryFilterData filterData_(PxFilterData(), queryFlags_);
    const PxHitFlags hitFlags_ =
        PxHitFlag::eDEFAULT | PxHitFlag::eMESH_BOTH_SIDES;
    if (probeRadius > 0.001f)
    {
        PxSweepBuffer sweepHit_;
        const PxSphereGeometry probeGeometry_((std::max)(probeRadius, 0.001f));
        if (!scene_->sweep(
            probeGeometry_,
            PxTransform(origin_),
            direction_,
            distance_,
            sweepHit_,
            hitFlags_,
            filterData_,
            &queryFilter_))
        {
            return false;
        }
        hitDistance_ = sweepHit_.block.distance;
        return true;
    }

    PxRaycastBuffer rayHit_;
    if (!scene_->raycast(
        origin_,
        direction_,
        distance_,
        rayHit_,
        hitFlags_,
        filterData_,
        &queryFilter_))
    {
        return false;
    }
    hitDistance_ = rayHit_.block.distance;
    return true;
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
    ImGui::SeparatorText("LockOn Collision Lift");
    ImGui::Checkbox(
        "Enable LockOn Collision Lift",
        &enableLockOnCollisionLift);
    ImGui::Checkbox(
        "Reduce Lift When Arena Wall Camera Active",
        &reduceCollisionLiftWhenArenaWallCameraActive);
    ImGui::DragFloat(
        "Max Collision Lift Height",
        &maxCollisionLiftHeight,
        0.05f,
        0.0f,
        10.0f);
    ImGui::DragFloat(
        "Collision Lift Start Rate",
        &collisionLiftStartRate,
        0.01f,
        0.0f,
        0.95f);
    ImGui::DragFloat(
        "Collision Lift Up Speed",
        &collisionLiftUpSpeed,
        0.1f,
        0.1f,
        100.0f);
    ImGui::DragFloat(
        "Collision Lift Down Speed",
        &collisionLiftDownSpeed,
        0.1f,
        0.1f,
        100.0f);
    if (const std::shared_ptr<CameraLockOnComponent> lockOnCamera_ =
        lockOnCamera.lock();
        lockOnCamera_ && lockOnCamera_->IsLockOnEnabled())
    {
        ImGui::Text("Desired Distance: %.3f", debugDesiredDistance);
        ImGui::Text("Corrected Distance: %.3f", debugCorrectedDistance);
        ImGui::Text("Compression Rate: %.3f", debugCompressionRate);
        ImGui::Text("Current Collision Lift: %.3f", currentCollisionLift);
    }
}

void CameraCollisionComponent::Serialize(nlohmann::json& json_) const
{
    json_["ignoreTargetName"] = ignoreTargetName;
    json_["ignoreLockOnTargetName"] = ignoreLockOnTargetName;

    json_["collisionOffset"] = collisionOffset;
    json_["minimumCameraDistance"] = minimumCameraDistance;
    json_["probeRadius"] = probeRadius;
    json_["includeDynamicColliders"] = includeDynamicColliders;
    json_["enableLockOnCollisionLift"] = enableLockOnCollisionLift;
    json_["reduceCollisionLiftWhenArenaWallCameraActive"] =
        reduceCollisionLiftWhenArenaWallCameraActive;
    json_["maxCollisionLiftHeight"] = maxCollisionLiftHeight;
    json_["collisionLiftStartRate"] = collisionLiftStartRate;
    json_["collisionLiftUpSpeed"] = collisionLiftUpSpeed;
    json_["collisionLiftDownSpeed"] = collisionLiftDownSpeed;
}

void CameraCollisionComponent::Deserialize(const nlohmann::json& json_)
{
    ignoreTargetName = json_.value("ignoreTargetName", std::string{});
    ignoreLockOnTargetName = json_.value("ignoreLockOnTargetName", std::string{});

    collisionOffset = json_.value("collisionOffset", 0.15f);
    minimumCameraDistance = json_.value("minimumCameraDistance", 0.05f);
    probeRadius = json_.value("probeRadius", 0.3f);
    includeDynamicColliders = json_.value("includeDynamicColliders", true);
    enableLockOnCollisionLift =
        json_.value("enableLockOnCollisionLift", true);
    reduceCollisionLiftWhenArenaWallCameraActive =
        json_.value("reduceCollisionLiftWhenArenaWallCameraActive", true);
    maxCollisionLiftHeight =
        json_.value("maxCollisionLiftHeight", 3.0f);
    collisionLiftStartRate =
        json_.value("collisionLiftStartRate", 0.25f);
    collisionLiftUpSpeed =
        json_.value("collisionLiftUpSpeed", 14.0f);
    collisionLiftDownSpeed =
        json_.value("collisionLiftDownSpeed", 5.0f);

    collisionOffset = std::clamp(collisionOffset, 0.0f, 5.0f);
    minimumCameraDistance = (std::max)(minimumCameraDistance, 0.001f);
    probeRadius = (std::max)(probeRadius, 0.0f);
    maxCollisionLiftHeight = std::clamp(maxCollisionLiftHeight, 0.0f, 10.0f);
    collisionLiftStartRate = std::clamp(collisionLiftStartRate, 0.0f, 0.95f);
    collisionLiftUpSpeed = std::clamp(collisionLiftUpSpeed, 0.1f, 100.0f);
    collisionLiftDownSpeed = std::clamp(collisionLiftDownSpeed, 0.1f, 100.0f);

    ignoreTarget.reset();
    ignoreLockOnTarget.reset();
    lockOnCamera.reset();
    currentCollisionLift = 0.0f;
}

REGISTER_COMPONENT(CameraCollisionComponent, "CameraCollisionComponent")
