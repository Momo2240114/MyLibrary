#include "PrecompiledHeader.h"

#include "CameraCollisionComponent.h"

#include "CameraComponent.h"
#include "CameraLockOnComponent.h"
#include "SpecialAttackCameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/ContainmentVolumeComponent.h"
#include "Engine/Physics/PhysXSystem.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace physx;

namespace
{
    struct CameraCollisionLayerView
    {
        // Editor表示名と実Layerの対応。
        const char* name;
        LayerType layer;
    };

    constexpr CameraCollisionLayerView cameraCollisionLayerViews_[] =
    {
        { "Player", LayerType::Player },
        { "Enemy", LayerType::Enemy },
        { "PlayerBullet", LayerType::PlayerBullet },
        { "EnemyBullet", LayerType::EnemyBullet },
        { "UI", LayerType::UI },
        { "Environment", LayerType::Environment },
        { "SneakObject", LayerType::SneakObject },
        { "Terrain", LayerType::Terrain },
        { "Wall", LayerType::Wall },
        { "Item", LayerType::Item },
    };

    const char* GetCameraCollisionLayerName(uint32_t layer_)
    {
        if (layer_ == static_cast<uint32_t>(LayerType::Default))
        {
            return "Default";
        }

        for (const CameraCollisionLayerView& layerView_ : cameraCollisionLayerViews_)
        {
            if (layer_ == static_cast<uint32_t>(layerView_.layer))
            {
                return layerView_.name;
            }
        }

        return "Unknown";
    }

    ColliderComponent* GetColliderFromActor(const PxRigidActor* actor_)
    {
        // PhysX ActorのuserDataにはColliderComponentを入れている想定。
        if (!actor_ || !actor_->userData)
        {
            return nullptr;
        }
        return static_cast<ColliderComponent*>(actor_->userData);
    }

    uint32_t GetShapeLayer(const PxShape* shape_)
    {
        // Query用FilterDataを優先し、未設定ならSimulation側のLayerを見る。
        if (!shape_)
        {
            return static_cast<uint32_t>(LayerType::Default);
        }

        PxFilterData filterData_ = shape_->getQueryFilterData();
        if (filterData_.word0 == 0u)
        {
            filterData_ = shape_->getSimulationFilterData();
        }
        return filterData_.word0;
    }

    bool IsLayerMaskAccepted(uint32_t layer_, uint32_t layerMask_)
    {
        return layer_ != static_cast<uint32_t>(LayerType::Default) &&
            (layerMask_ & layer_) != 0u;
    }

    bool IsCameraCollisionLayerAccepted(
        const PxShape* shape_,
        const PxRigidActor* actor_,
        uint32_t layerMask_)
    {
        // Shape/ActorがCamera Collision対象Layerかを判定する。
        if (!shape_ || !actor_)
        {
            return false;
        }

        PxFilterData filterData_ = shape_->getQueryFilterData();
        if ((filterData_.word1 & ContainmentVolumeComponent::IGNORE_CAMERA_QUERY_BIT) != 0u)
        {
            return false;
        }
        if (filterData_.word0 == 0u)
        {
            filterData_ = shape_->getSimulationFilterData();
        }

        const uint32_t shapeLayer_ = filterData_.word0;
        if (IsLayerMaskAccepted(shapeLayer_, layerMask_))
        {
            return true;
        }

        if (shapeLayer_ == static_cast<uint32_t>(LayerType::Default))
        {
            // Default LayerのStatic Physical Colliderだけは、旧設定互換としてCamera衝突対象にする。
            return actor_->is<PxRigidStatic>() &&
                filterData_.word2 ==
                static_cast<uint32_t>(ColliderComponent::CollisionRole::PHYSICAL);
        }

        return false;
    }

    class CameraQueryFilter : public PxQueryFilterCallback
    {
    public:
        CameraQueryFilter(
            const std::vector<const void*>& ignoredUserData_,
            uint32_t layerMask_)
            : ignoredUserData(ignoredUserData_),
            layerMask(layerMask_)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData_,
            const PxShape* shape_,
            const PxRigidActor* actor_,
            PxHitFlags& queryFlags_) override
        {
            // Scene Query不可、Trigger、Mask外、Ignore対象はSweep/Raycastから除外する。
            (void)filterData_;
            (void)queryFlags_;
            if (!shape_ || !actor_)
            {
                return PxQueryHitType::eNONE;
            }
            if (!shape_->getFlags().isSet(PxShapeFlag::eSCENE_QUERY_SHAPE))
            {
                return PxQueryHitType::eNONE;
            }
            if (shape_->getFlags().isSet(PxShapeFlag::eTRIGGER_SHAPE))
            {
                return PxQueryHitType::eNONE;
            }

            if (!IsCameraCollisionLayerAccepted(shape_, actor_, layerMask))
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
        uint32_t layerMask = 0u;
    };

    void CollectIgnoredUserData(
        const std::shared_ptr<GameObject>& object_,
        std::vector<const void*>& userData_)
    {
        // 指定Object配下のColliderComponentをPhysX userData比較用に集める。
        if (!object_)
        {
            return;
        }
        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            if (collider_)
            {
                userData_.push_back(collider_.get());
            }
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

uint32_t CameraCollisionComponent::GetDefaultCollisionLayerMask()
{
    // 通常Cameraが避けるべき環境系Layerだけを標準対象にする。
    return
        static_cast<uint32_t>(LayerType::Environment) |
        static_cast<uint32_t>(LayerType::Terrain) |
        static_cast<uint32_t>(LayerType::Wall);
}

void CameraCollisionComponent::Start()
{
    ResolveReferences();
}

void CameraCollisionComponent::LateUpdate(float deltaTime_)
{
    ResolveReferences();
    // 他Camera制御やDebug用途でSuppressed中は補正しない。
    if (collisionSuppressed)
    {
        return;
    }

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_ || !PhysXSystem::GetScene())
    {
        return;
    }

    const auto owner_ = GetGameObject();
    const auto special_ = owner_ ? owner_->GetComponent<SpecialAttackCameraComponent>() : nullptr;
    const bool specialTracking_ = special_ && special_->HasTrackingPose();
    const auto lockOnCamera_ = lockOnCamera.lock();
    const DirectX::XMFLOAT3 focus_ = camera_->GetFocus();
    // 通常ロックオンでは敵側へ動く注視点を避け、TPSと同じプレイヤー基準で遮蔽を探す。
    const DirectX::XMFLOAT3 start_ = !specialTracking_ && lockOnCamera_ && lockOnCamera_->HasTrackingPose()
        ? lockOnCamera_->GetCollisionAnchor()
        : focus_;
    const DirectX::XMFLOAT3 desiredPosition_ = camera_->GetEye();
    // 衝突判定の基準点からカメラまでをスイープする。
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
    if (desiredDistance_ <= 0.001f)
    {
        return;
    }

    std::vector<const void*> ignoredUserData_;
    // 自分自身、追従Target、LockOn対象などはCamera遮蔽物として扱わない。
    CollectIgnoredUserData(GetGameObject(), ignoredUserData_);
    CollectIgnoredUserData(ignoreTarget.lock(), ignoredUserData_);
    CollectIgnoredUserData(ignoreLockOnTarget.lock(), ignoredUserData_);
    if (specialTracking_)
    {
        CollectIgnoredUserData(special_->GetPlayerTarget(), ignoredUserData_);
        CollectIgnoredUserData(special_->GetAttackTarget(), ignoredUserData_);
    }
    else if (lockOnCamera_)
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
    if (hasHit_ && !std::isfinite(hitDistance_))
    {
        return;
    }

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
    // Hitした場合はHit点より少し手前、Hitしない場合は理想距離をそのまま使う。
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

    const bool canLift_ = enableLockOnCollisionLift && specialTracking_;
    const float arenaWallCameraRate_ = reduceCollisionLiftWhenArenaWallCameraActive && specialTracking_ && special_->IsWallCameraActive()
        ? special_->GetWallCameraRate()
        : 0.0f;
    const float safeLiftStartRate_ = std::clamp(
        collisionLiftStartRate,
        0.0f,
        0.95f);
    const float liftRate_ = canLift_
        ? std::clamp(
            // Cameraが圧縮され始めた量に応じて、上方向への逃がし量を増やす。
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
        // 持ち上げ/戻しは速度を分け、壁に入った瞬間は速く、戻りはやや遅くする。
        1.0f - std::exp(-(std::max)(liftSpeed_, 0.1f) * safeDeltaTime_),
        0.0f,
        1.0f);
    currentCollisionLift = specialTracking_
        ? std::lerp(currentCollisionLift, targetCollisionLift_, smoothingRate_)
        : 0.0f;

    DirectX::XMFLOAT3 finalPosition_ = limitedPosition_;
    if (canLift_ && currentCollisionLift > 0.001f)
    {
        // 上方向へ逃がす際も、天井などに当たらないか追加Sweepする。
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
        // 何も補正していないFrameではCamera Poseを書き直さない。
        return;
    }

    if (specialTracking_ &&
        special_->ApplyCollisionComposition(
            finalPosition_,
            start_,
            debugCompressionRate,
            currentCollisionLift,
            deltaTime_))
    {
        // 特殊攻撃カメラが壁際の構図まで補正した場合は、その姿勢を使う。
        return;
    }
    ApplyCameraPose(finalPosition_, focus_);
}

void CameraCollisionComponent::ResolveReferences()
{
    // 必要なComponentとIgnore対象を、失効している場合だけ再取得する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

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
    BS_PROFILE_SCOPE("CameraCollisionComponent::SweepCameraSegment");

    PxScene* scene_ = PhysXSystem::GetScene();
    if (!scene_)
    {
        return false;
    }

    const float distance_ = DistanceFloat3(startPosition_, endPosition_);
    if (distance_ <= 0.001f)
    {
        return false;
    }

    debugLastHitObjectName.clear();
    debugLastHitColliderName.clear();
    debugLastHitLayer = 0u;
    debugLastHitAccepted = false;

    const PxVec3 origin_(startPosition_.x, startPosition_.y, startPosition_.z);
    // PhysX Query用にSegmentを始点、正規化方向、距離へ分解する。
    const PxVec3 direction_(
        (endPosition_.x - startPosition_.x) / distance_,
        (endPosition_.y - startPosition_.y) / distance_,
        (endPosition_.z - startPosition_.z) / distance_);

    CameraQueryFilter queryFilter_(ignoredUserData_, cameraCollisionLayerMask);

    PxQueryFlags queryFlags_ = PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;
    // 動的ColliderをCamera衝突へ含めるかは設定で切り替える。
    if (includeDynamicColliders)
    {
        queryFlags_ |= PxQueryFlag::eDYNAMIC;
    }
    const PxQueryFilterData filterData_(
        PxFilterData(cameraCollisionLayerMask, 0u, 0u, 0u),
        queryFlags_);
    const PxHitFlags hitFlags_ =
        PxHitFlag::eDEFAULT | PxHitFlag::eMESH_BOTH_SIDES;
    bool hasHit_ = false;
    float nearestHitDistance_ = distance_;

    if (probeRadius > 0.001f)
    {
        // Sphere Sweepを先に行い、Cameraに厚みを持たせた衝突補正をする。
        PxSweepBuffer sweepHit_;
        const PxSphereGeometry probeGeometry_((std::max)(probeRadius, 0.001f));
        if (scene_->sweep(
            probeGeometry_,
            PxTransform(origin_),
            direction_,
            distance_,
            sweepHit_,
            hitFlags_,
            filterData_,
            &queryFilter_))
        {
            hasHit_ = true;
            nearestHitDistance_ = sweepHit_.block.distance;
            debugLastHitAccepted = true;
            debugLastHitLayer = GetShapeLayer(sweepHit_.block.shape);
            if (ColliderComponent* collider_ =
                GetColliderFromActor(sweepHit_.block.actor))
            {
                debugLastHitColliderName = collider_->GetColliderName();
                debugLastHitObjectName = collider_->GetCollisionOwnerObjectName();
            }
        }
    }

    PxRaycastBuffer rayHit_;
    // Sphere Sweepで取りこぼす細い形状のため、中心Raycastも併用する。
    if (scene_->raycast(
        origin_,
        direction_,
        distance_,
        rayHit_,
        hitFlags_,
        filterData_,
        &queryFilter_))
    {
        if (!hasHit_ || rayHit_.block.distance < nearestHitDistance_)
        {
            hasHit_ = true;
            nearestHitDistance_ = rayHit_.block.distance;
            debugLastHitAccepted = true;
            debugLastHitLayer = GetShapeLayer(rayHit_.block.shape);
            if (ColliderComponent* collider_ = GetColliderFromActor(rayHit_.block.actor))
            {
                debugLastHitColliderName = collider_->GetColliderName();
                debugLastHitObjectName = collider_->GetCollisionOwnerObjectName();
            }
        }
    }

    if (!hasHit_)
    {
        return false;
    }

    hitDistance_ = nearestHitDistance_;
    return true;
}

void CameraCollisionComponent::DrawCollisionLayerMaskGui()
{
    ImGui::SeparatorText("Camera Collision Layer Mask");

    // LayerごとのCheckboxをbit maskへ反映する。
    for (const CameraCollisionLayerView& layerView_ : cameraCollisionLayerViews_)
    {
        const uint32_t layerMask_ = static_cast<uint32_t>(layerView_.layer);
        bool enabled_ = (cameraCollisionLayerMask & layerMask_) != 0u;
        if (ImGui::Checkbox(layerView_.name, &enabled_))
        {
            if (enabled_)
            {
                cameraCollisionLayerMask |= layerMask_;
            }
            else
            {
                cameraCollisionLayerMask &= ~layerMask_;
            }
        }
    }

    if (ImGui::Button("Use Camera Default Layers"))
    {
        cameraCollisionLayerMask = GetDefaultCollisionLayerMask();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Camera Layers"))
    {
        cameraCollisionLayerMask = 0u;
    }

    ImGui::Text("Layer Mask: %u", cameraCollisionLayerMask);
}

void CameraCollisionComponent::ApplyCameraPose(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_)
{
    // CameraのLookAt姿勢をTransformにも反映し、GameObject上のCamera位置と実Viewを同期する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        return;
    }

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

void CameraCollisionComponent::SetCollisionSuppressed(bool suppressed_)
{
    collisionSuppressed = suppressed_;
    if (!collisionSuppressed)
    {
        // 再開時に古い持ち上げ量や圧縮率が残らないよう初期化する。
        currentCollisionLift = 0.0f;
        debugCompressionRate = 0.0f;
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
    ImGui::Checkbox("Collision Suppressed", &collisionSuppressed);
    DrawCollisionLayerMaskGui();
    ImGui::SeparatorText("Special Attack Camera Lift");
    ImGui::Checkbox(
        "Enable Special Attack Camera Lift",
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
    ImGui::Text("Desired Distance: %.3f", debugDesiredDistance);
    ImGui::Text("Corrected Distance: %.3f", debugCorrectedDistance);
    ImGui::Text("Compression Rate: %.3f", debugCompressionRate);
    ImGui::Text("Current Collision Lift: %.3f", currentCollisionLift);
    ImGui::SeparatorText("Last Camera Collision Hit");
    ImGui::Text(
        "Object: %s",
        debugLastHitObjectName.empty() ? "None" : debugLastHitObjectName.c_str());
    ImGui::Text(
        "Collider: %s",
        debugLastHitColliderName.empty() ? "None" : debugLastHitColliderName.c_str());
    ImGui::Text(
        "Layer: %s (%u)",
        GetCameraCollisionLayerName(debugLastHitLayer),
        debugLastHitLayer);
    ImGui::Text("Accepted By Mask: %s", debugLastHitAccepted ? "Yes" : "No");
}

void CameraCollisionComponent::Serialize(nlohmann::json& json_) const
{
    if (runtimeSettingsActive)
    {
        json_ = originalRuntimeSettings;
        return;
    }
    // Debug値や現在Lift量はRuntime状態なので保存しない。
    json_["ignoreTargetName"] = ignoreTargetName;
    json_["ignoreLockOnTargetName"] = ignoreLockOnTargetName;

    json_["collisionOffset"] = collisionOffset;
    json_["cameraCollisionLayerMask"] = cameraCollisionLayerMask;
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
    runtimeSettingsActive = false;
    originalRuntimeSettings = nlohmann::json{};
    // 旧collisionLayerMask Keyにも対応し、過去Sceneから設定を引き継ぐ。
    ignoreTargetName = json_.value("ignoreTargetName", std::string{});
    ignoreLockOnTargetName = json_.value("ignoreLockOnTargetName", std::string{});

    collisionOffset = json_.value("collisionOffset", 0.15f);
    cameraCollisionLayerMask = json_.value(
        "cameraCollisionLayerMask",
        json_.value("collisionLayerMask", GetDefaultCollisionLayerMask()));
    if (cameraCollisionLayerMask == 0u)
    {
        cameraCollisionLayerMask = GetDefaultCollisionLayerMask();
    }
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

// 元の設定は一度だけ保存し、第二形態のカメラ設定を実行時へ適用する。
void CameraCollisionComponent::ApplyRuntimeSettings(const nlohmann::json& settings_)
{
    BS_PROFILE_SCOPE("CameraCollisionComponent::ApplyRuntimeSettings");
    nlohmann::json original_;
    Serialize(original_);
    Deserialize(settings_);
    originalRuntimeSettings = std::move(original_);
    runtimeSettingsActive = true;
}

void CameraCollisionComponent::ClearRuntimeSettings()
{
    if (!runtimeSettingsActive)
    {
        return;
    }
    const auto original_ = originalRuntimeSettings;
    Deserialize(original_);
}
