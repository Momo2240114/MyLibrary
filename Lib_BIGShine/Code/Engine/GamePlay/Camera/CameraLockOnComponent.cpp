#include "PrecompiledHeader.h"

#include "CameraLockOnComponent.h"

#include "CameraComponent.h"
#include "CameraManager.h"
#include "LockOnTargetComponent.h"
#include "ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
    DirectX::XMFLOAT3 AddFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x + right_.x, left_.y + right_.y, left_.z + right_.z };
    }

    DirectX::XMFLOAT3 LerpFloat3(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        float rate_)
    {
        return
        {
            start_.x + (end_.x - start_.x) * rate_,
            start_.y + (end_.y - start_.y) * rate_,
            start_.z + (end_.z - start_.z) * rate_
        };
    }

    float ExpSmoothingRate(float speed_, float deltaTime_)
    {
        return 1.0f - std::exp(-speed_ * deltaTime_);
    }

    DirectX::XMFLOAT3 NormalizeFloat3(
        const DirectX::XMFLOAT3& value_)
    {
        const float length_ = std::sqrt(
            value_.x * value_.x +
            value_.y * value_.y +
            value_.z * value_.z);
        if (length_ <= 0.0001f)
        {
            return { 0.0f, 0.0f, 1.0f };
        }
        return
        {
            value_.x / length_,
            value_.y / length_,
            value_.z / length_
        };
    }
}

void CameraLockOnComponent::Start()
{
    ResolveReferences();
    if (const std::shared_ptr<CameraComponent> camera_ = camera.lock())
    {
        baseFovY = camera_->GetFovY();
        currentCollisionFov = baseFovY;
        hasBaseFov = true;
    }
    RegisterActions();
}

void CameraLockOnComponent::LateUpdate(float deltaTime_)
{
    ResolveReferences();
    if (!lockOnEnabled)
    {
        ReleaseThirdPersonCamera();
        return;
    }

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> enemy_ = lockOnTarget.lock();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
        thirdPersonCamera.lock();
    if (!player_ || !enemy_ || !camera_ ||
        !player_->transform || !enemy_->transform)
    {
        SetLockOnEnabled(false);
        return;
    }

    if (thirdPerson_)
    {
        thirdPerson_->SetControlSuppressed(true);
        ownsThirdPersonControl = true;
    }
    if (!hasBaseFov)
    {
        baseFovY = camera_->GetFovY();
        currentCollisionFov = baseFovY;
        hasBaseFov = true;
    }

    const LockOnDistanceInfo distanceInfo_ = CalculateLockOnDistanceInfo();

    if (!distanceInfo_.isValid)
    {
        SetLockOnEnabled(false);
        return;
    }

    const DirectX::XMFLOAT3 playerPosition_ = distanceInfo_.playerPosition;
    const DirectX::XMFLOAT3 enemyPosition_ = distanceInfo_.targetPosition;
    const float differenceX_ = distanceInfo_.difference.x;
    const float differenceY_ = distanceInfo_.difference.y;
    const float differenceZ_ = distanceInfo_.difference.z;
    const float horizontalDistance_ = distanceInfo_.horizontalDistance;
    const float targetDistance_ = distanceInfo_.targetDistance;

    if (targetDistance_ > maxDistance)
    {
        SetLockOnEnabled(false);
        return;
    }

    DirectX::XMFLOAT3 backDirection_{};
    if (horizontalDistance_ > 0.0001f)
    {
        backDirection_ =
        {
            differenceX_ / horizontalDistance_,
            0.0f,
            differenceZ_ / horizontalDistance_
        };
    }
    else
    {
        const DirectX::XMFLOAT3 cameraFront_ = camera_->GetFront();
        const float frontLength_ = std::sqrt(
            cameraFront_.x * cameraFront_.x + cameraFront_.z * cameraFront_.z);
        backDirection_ = frontLength_ > 0.0001f
            ? DirectX::XMFLOAT3{
                -cameraFront_.x / frontLength_,
                0.0f,
                -cameraFront_.z / frontLength_ }
            : DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f };
    }

    const DirectX::XMFLOAT3 sideDirection_
    {
        -backDirection_.z,
        0.0f,
        backDirection_.x
    };
    const float desiredDistance_ = std::clamp(
        baseDistance + horizontalDistance_ * distanceScale,
        minCameraDistance,
        maxCameraDistance);
    const float desiredHeight_ = cameraHeight +
        horizontalDistance_ * separationHeightScale +
        (std::max)(enemyPosition_.y - playerPosition_.y, 0.0f) *
            heightDifferenceScale;
    const DirectX::XMFLOAT3 desiredPosition_
    {
        playerPosition_.x + backDirection_.x * desiredDistance_ +
            sideDirection_.x * sideOffset * currentSide,
        playerPosition_.y + desiredHeight_,
        playerPosition_.z + backDirection_.z * desiredDistance_ +
            sideDirection_.z * sideOffset * currentSide
    };

    const float focusRate_ = std::clamp(
        horizontalDistance_ / (std::max)(closeDistance, 0.001f),
        0.0f,
        1.0f);
    const float enemyFocusWeight_ = std::lerp(
        closeFocusBias,
        farFocusBias,
        focusRate_);
    const DirectX::XMFLOAT3 desiredLookAt_ = LerpFloat3(
        playerPosition_,
        enemyPosition_,
        enemyFocusWeight_);

    const std::shared_ptr<GameObject> previousTarget_ =
        previousLockOnTarget.lock();
    if (!hasTrackingPose || previousTarget_ != enemy_)
    {
        interpolatedPosition = camera_->GetEye();
        interpolatedLookAt = camera_->GetFocus();
        previousLockOnTarget = enemy_;
        hasTrackingPose = true;
    }

    interpolatedPosition = LerpFloat3(
        interpolatedPosition,
        desiredPosition_,
        ExpSmoothingRate(positionInterpolationSpeed, deltaTime_));
    interpolatedLookAt = LerpFloat3(
        interpolatedLookAt,
        desiredLookAt_,
        ExpSmoothingRate(lookAtInterpolationSpeed, deltaTime_));
    if (!hasCollisionComposition)
    {
        collisionLookAt = interpolatedLookAt;
    }
    currentCollisionFov = std::lerp(
        currentCollisionFov,
        baseFovY,
        ExpSmoothingRate(collisionFovInterpolationSpeed, deltaTime_));
    camera_->SetFov(currentCollisionFov);
    hasCollisionComposition = false;
    ApplyCameraPose(interpolatedPosition, interpolatedLookAt);
    if (thirdPerson_)
    {
        thirdPerson_->SynchronizeViewAngles(
            interpolatedPosition,
            interpolatedLookAt);
    }
}

void CameraLockOnComponent::SetLockOnEnabled(bool enabled_)
{
    lockOnEnabled = enabled_ && !lockOnTarget.expired();
    ResetTracking();
    if (!lockOnEnabled)
    {
        if (const std::shared_ptr<CameraComponent> camera_ = camera.lock();
            camera_ && hasBaseFov)
        {
            currentCollisionFov = baseFovY;
            camera_->SetFov(baseFovY);
        }
        ReleaseThirdPersonCamera();
    }
}

void CameraLockOnComponent::SetPlayerTarget(
    const std::shared_ptr<GameObject>& playerTarget_)
{
    playerTarget = playerTarget_;
    playerTargetName = playerTarget_ && playerTarget_->nameComponent
        ? playerTarget_->nameComponent->name
        : std::string{};
    ResetTracking();
}

void CameraLockOnComponent::SetLockOnTarget(
    const std::shared_ptr<GameObject>& lockOnTarget_)
{
    lockOnTarget = lockOnTarget_;
    lockOnTargetName = lockOnTarget_ && lockOnTarget_->nameComponent
        ? lockOnTarget_->nameComponent->name
        : std::string{};
    if (!lockOnTarget_)
    {
        SetLockOnEnabled(false);
        return;
    }
    ResetTracking();
}

std::shared_ptr<GameObject> CameraLockOnComponent::FindBestLockOnTarget(
    const std::shared_ptr<GameObject>& playerTarget_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager || !playerTarget_ || !playerTarget_->transform)
    {
        return nullptr;
    }

    const DirectX::XMFLOAT3 playerPosition_ =
        GetFocusPosition(playerTarget_, playerOffset);
    DirectX::XMFLOAT3 cameraForward_{ 0.0f, 0.0f, 1.0f };
    if (CameraManager* cameraManager_ = owner_->manager->GetCameraManager())
    {
        if (const std::shared_ptr<CameraComponent> activeCamera_ =
            cameraManager_->GetActiveCamera())
        {
            cameraForward_ = activeCamera_->GetFront();
        }
    }

    float forwardLength_ = std::sqrt(
        cameraForward_.x * cameraForward_.x +
        cameraForward_.z * cameraForward_.z);
    if (forwardLength_ <= 0.0001f)
    {
        cameraForward_ = { 0.0f, 0.0f, 1.0f };
        forwardLength_ = 1.0f;
    }
    cameraForward_.x /= forwardLength_;
    cameraForward_.z /= forwardLength_;

    std::shared_ptr<GameObject> bestTarget_;
    float bestScore_ = (std::numeric_limits<float>::max)();
    for (const std::shared_ptr<GameObject>& candidate_ : owner_->manager->GetObjects())
    {
        if (!candidate_ || candidate_ == playerTarget_ || !candidate_->transform ||
            !candidate_->GetComponent<LockOnTargetComponent>())
        {
            continue;
        }

        const DirectX::XMFLOAT3 targetPosition_ =
            GetFocusPosition(candidate_, targetOffset);
        const float differenceX_ = targetPosition_.x - playerPosition_.x;
        const float differenceZ_ = targetPosition_.z - playerPosition_.z;
        const float horizontalDistance_ = std::sqrt(
            differenceX_ * differenceX_ + differenceZ_ * differenceZ_);
        if (horizontalDistance_ <= 0.0001f || horizontalDistance_ > maxDistance)
        {
            continue;
        }

        const float directionX_ = differenceX_ / horizontalDistance_;
        const float directionZ_ = differenceZ_ / horizontalDistance_;
        const float centerDot_ = std::clamp(
            cameraForward_.x * directionX_ + cameraForward_.z * directionZ_,
            -1.0f,
            1.0f);
        const float centerPenalty_ = (1.0f - centerDot_) * maxDistance;
        const float score_ = horizontalDistance_ + centerPenalty_;
        if (score_ < bestScore_)
        {
            bestScore_ = score_;
            bestTarget_ = candidate_;
        }
    }

    return bestTarget_;
}

bool CameraLockOnComponent::ToggleLockOnForPlayer(
    const std::shared_ptr<GameObject>& playerTarget_)
{
    if (lockOnEnabled)
    {
        SetLockOnEnabled(false);
        return false;
    }

    SetPlayerTarget(playerTarget_);
    const std::shared_ptr<GameObject> target_ = FindBestLockOnTarget(playerTarget_);
    SetLockOnTarget(target_);
    SetLockOnEnabled(target_ != nullptr);
    return lockOnEnabled;
}

bool CameraLockOnComponent::ApplyCollisionComposition(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& baseLookAtPosition_,
    float compressionRate_,
    float collisionLift_,
    float deltaTime_)
{
    ResolveReferences();
    if (!lockOnEnabled) return false;

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> target_ = lockOnTarget.lock();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!player_ || !target_ || !camera_ ||
        !player_->transform || !target_->transform)
    {
        return false;
    }

    const DirectX::XMFLOAT3 playerTopPosition_ =
        GetFocusPosition(player_, playerOffset);
    const DirectX::XMFLOAT3 playerBottomPosition_ =
        GetPlayerBottomPosition(player_);
    const DirectX::XMFLOAT3 targetPosition_ =
        GetFocusPosition(target_, targetOffset);
    const float safeLiftRate_ = std::clamp(
        collisionLift_ /
            (std::max)(collisionLookAtSearchRadius, 0.001f),
        0.0f,
        1.0f);
    const float safeCompressionRate_ = std::clamp(
        compressionRate_,
        0.0f,
        1.0f);

    const DirectX::XMFLOAT3 targetLookAt_ = CalculateCollisionLookAt(
        cameraPosition_,
        baseLookAtPosition_,
        playerTopPosition_,
        playerBottomPosition_,
        targetPosition_,
        safeCompressionRate_,
        safeLiftRate_);
    if (!hasCollisionComposition)
    {
        collisionLookAt = baseLookAtPosition_;
        collisionCameraPosition = cameraPosition_;
        hasCollisionComposition = true;
    }
    collisionLookAt = LerpFloat3(
        collisionLookAt,
        targetLookAt_,
        ExpSmoothingRate(framingSmoothSpeed, deltaTime_));
    const DirectX::XMFLOAT3 targetCameraPosition_ =
        CalculateFramingCameraPosition(
            cameraPosition_,
            collisionLookAt,
            playerTopPosition_,
            playerBottomPosition_,
            targetPosition_,
            safeCompressionRate_);
    collisionCameraPosition = LerpFloat3(
        collisionCameraPosition,
        targetCameraPosition_,
        ExpSmoothingRate(framingSmoothSpeed, deltaTime_));

    const float targetFov_ = CalculateCollisionFov(
        collisionCameraPosition,
        collisionLookAt,
        playerTopPosition_,
        playerBottomPosition_,
        targetPosition_,
        safeCompressionRate_);
    currentCollisionFov = std::lerp(
        currentCollisionFov,
        targetFov_,
        ExpSmoothingRate(collisionFovInterpolationSpeed, deltaTime_));
    camera_->SetFov(currentCollisionFov);
    const bool playerVisible_ = ProjectToScreenPosition(
        collisionCameraPosition,
        collisionLookAt,
        playerTopPosition_,
        currentCollisionFov,
        debugPlayerScreen);
    const bool playerBottomVisible_ = ProjectToScreenPosition(
        collisionCameraPosition,
        collisionLookAt,
        playerBottomPosition_,
        currentCollisionFov,
        debugPlayerBottomScreen);
    const bool targetVisible_ = ProjectToScreenPosition(
        collisionCameraPosition,
        collisionLookAt,
        targetPosition_,
        currentCollisionFov,
        debugTargetScreen);
    debugHorizontalCorrection =
        !playerVisible_ ||
        !targetVisible_ ||
        debugPlayerScreen.x < horizontalSafeMin ||
        debugPlayerScreen.x > horizontalSafeMax ||
        debugPlayerBottomScreen.x < horizontalSafeMin ||
        debugPlayerBottomScreen.x > horizontalSafeMax ||
        debugTargetScreen.x < horizontalSafeMin ||
        debugTargetScreen.x > horizontalSafeMax;
    debugVerticalCorrection =
        !playerVisible_ ||
        !playerBottomVisible_ ||
        !targetVisible_ ||
        debugPlayerScreen.y < verticalSafeMin ||
        debugPlayerScreen.y > verticalSafeMax ||
        debugPlayerBottomScreen.y < verticalSafeMin ||
        debugPlayerBottomScreen.y > verticalSafeMax ||
        debugTargetScreen.y < verticalSafeMin ||
        debugTargetScreen.y > verticalSafeMax;
    debugPlayerBottomCorrection =
        !playerBottomVisible_ ||
        debugPlayerBottomScreen.y > playerBottomLimit;
    debugFovCorrection = currentCollisionFov > baseFovY + 0.01f;
    ApplyCameraPose(collisionCameraPosition, collisionLookAt);
    return true;
}

void CameraLockOnComponent::ResolveReferences()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (camera.expired()) camera = owner_->GetComponent<CameraComponent>();
    if (thirdPersonCamera.expired())
    {
        thirdPersonCamera = owner_->GetComponent<ThirdPersonCameraComponent>();
    }
    if (!owner_->manager) return;
    if (playerTarget.expired() && !playerTargetName.empty())
    {
        playerTarget = owner_->manager->Find(playerTargetName);
    }
    if (lockOnTarget.expired() && !lockOnTargetName.empty())
    {
        lockOnTarget = owner_->manager->Find(lockOnTargetName);
    }
}

void CameraLockOnComponent::RegisterActions()
{
    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RegisterAction("Camera", "LockOn_Enable", [this]()
    {
        ResolveReferences();
        SetLockOnEnabled(true);
    });
    registry_.RegisterAction("Camera", "LockOn_Disable", [this]()
    {
        SetLockOnEnabled(false);
    });
    registry_.RegisterAction("Camera", "LockOn_Toggle", [this]()
    {
        ResolveReferences();
        SetLockOnEnabled(!lockOnEnabled);
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchSide", [this]()
    {
        currentSide = currentSide >= 0.0f ? -1.0f : 1.0f;
    });
}

void CameraLockOnComponent::ResetTracking()
{
    hasTrackingPose = false;
    hasCollisionComposition = false;
    previousLockOnTarget.reset();
}

void CameraLockOnComponent::ReleaseThirdPersonCamera()
{
    if (const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
        thirdPersonCamera.lock())
    {
        if (!ownsThirdPersonControl) return;
        thirdPerson_->SetControlSuppressed(false);
        thirdPerson_->BeginReturnBlendFromCurrentCamera();
        ownsThirdPersonControl = false;
        return;
    }
    ownsThirdPersonControl = false;
}

void CameraLockOnComponent::ApplyCameraPose(
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

DirectX::XMFLOAT3 CameraLockOnComponent::GetFocusPosition(
    const std::shared_ptr<GameObject>& object_,
    const DirectX::XMFLOAT3& fallbackOffset_) const
{
    if (!object_ || !object_->transform) return fallbackOffset_;
    if (const std::shared_ptr<LockOnTargetComponent> lockTarget_ =
        object_->GetComponent<LockOnTargetComponent>())
    {
        return lockTarget_->GetFocusPosition();
    }
    return AddFloat3(object_->transform->GetWorldPosition(), fallbackOffset_);
}

DirectX::XMFLOAT3 CameraLockOnComponent::GetPlayerBottomPosition(
    const std::shared_ptr<GameObject>& object_) const
{
    if (!object_ || !object_->transform) return playerBottomOffset;
    return AddFloat3(object_->transform->GetWorldPosition(), playerBottomOffset);
}

DirectX::XMFLOAT3 CameraLockOnComponent::CalculateWeightedLookAt(
    const DirectX::XMFLOAT3& playerPosition_,
    const DirectX::XMFLOAT3& targetPosition_) const
{
    const float safePlayerWeight_ = (std::max)(playerFocusWeight, 0.0f);
    const float safeTargetWeight_ = (std::max)(targetFocusWeight, 0.0f);
    const float totalWeight_ = (std::max)(
        safePlayerWeight_ + safeTargetWeight_,
        0.001f);
    return
    {
        (playerPosition_.x * safePlayerWeight_ +
            targetPosition_.x * safeTargetWeight_) / totalWeight_,
        (playerPosition_.y * safePlayerWeight_ +
            targetPosition_.y * safeTargetWeight_) / totalWeight_,
        (playerPosition_.z * safePlayerWeight_ +
            targetPosition_.z * safeTargetWeight_) / totalWeight_
    };
}

DirectX::XMFLOAT3 CameraLockOnComponent::CalculateCollisionLookAt(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& baseLookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float compressionRate_,
    float liftRate_) const
{
    const DirectX::XMFLOAT3 weightedLookAt_ = CalculateWeightedLookAt(
        playerTopPosition_,
        targetPosition_);
    const float lowerHeight_ =
        collisionLookAtLowerHeight *
        (std::max)(compressionRate_, liftRate_);

    DirectX::XMFLOAT2 bottomScreen_{};
    ProjectToScreenPosition(
        cameraPosition_,
        baseLookAtPosition_,
        playerBottomPosition_,
        baseFovY,
        bottomScreen_);
    const float bottomOverflow_ =
        (std::max)(bottomScreen_.y - playerBottomLimit, 0.0f);
    const float footLowerHeight_ =
        bottomOverflow_ *
        verticalFocusCorrectionStrength *
        (1.0f + compressionRate_) +
        lowerHeight_;

    DirectX::XMFLOAT3 loweredBase_ = baseLookAtPosition_;
    loweredBase_.y -= footLowerHeight_;
    DirectX::XMFLOAT3 weightedLowered_ = weightedLookAt_;
    weightedLowered_.y -= footLowerHeight_;
    DirectX::XMFLOAT3 playerBiased_ = LerpFloat3(
        weightedLookAt_,
        playerTopPosition_,
        std::clamp(verticalFocusCorrectionStrength * 0.25f, 0.0f, 0.75f));
    playerBiased_.y -= footLowerHeight_;

    const DirectX::XMFLOAT3 candidates_[4]
    {
        baseLookAtPosition_,
        weightedLookAt_,
        weightedLowered_,
        playerBiased_
    };

    DirectX::XMFLOAT3 bestLookAt_ = baseLookAtPosition_;
    float bestScore_ = (std::numeric_limits<float>::max)();
    for (const DirectX::XMFLOAT3& candidate_ : candidates_)
    {
        DirectX::XMFLOAT2 playerTopScreen_{};
        DirectX::XMFLOAT2 playerBottomScreen_{};
        DirectX::XMFLOAT2 targetScreen_{};
        const bool playerTopVisible_ = ProjectToScreenPosition(
            cameraPosition_,
            candidate_,
            playerTopPosition_,
            baseFovY,
            playerTopScreen_);
        const bool playerBottomVisible_ = ProjectToScreenPosition(
            cameraPosition_,
            candidate_,
            playerBottomPosition_,
            baseFovY,
            playerBottomScreen_);
        const bool targetVisible_ = ProjectToScreenPosition(
            cameraPosition_,
            candidate_,
            targetPosition_,
            baseFovY,
            targetScreen_);
        const float score_ = CalculateProjectionScore(
            playerTopScreen_,
            playerBottomScreen_,
            targetScreen_,
            playerTopVisible_,
            playerBottomVisible_,
            targetVisible_);
        if (score_ < bestScore_)
        {
            bestScore_ = score_;
            bestLookAt_ = candidate_;
        }
    }
    return bestLookAt_;
}

DirectX::XMFLOAT3 CameraLockOnComponent::CalculateFramingCameraPosition(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float compressionRate_) const
{
    DirectX::XMFLOAT2 playerTopScreen_{};
    DirectX::XMFLOAT2 playerBottomScreen_{};
    DirectX::XMFLOAT2 targetScreen_{};
    ProjectToScreenPosition(
        cameraPosition_,
        lookAtPosition_,
        playerTopPosition_,
        baseFovY,
        playerTopScreen_);
    ProjectToScreenPosition(
        cameraPosition_,
        lookAtPosition_,
        playerBottomPosition_,
        baseFovY,
        playerBottomScreen_);
    ProjectToScreenPosition(
        cameraPosition_,
        lookAtPosition_,
        targetPosition_,
        baseFovY,
        targetScreen_);

    const float playerBottomOverflow_ =
        (std::max)(playerBottomScreen_.y - playerBottomLimit, 0.0f);
    const float verticalOverflow_ = (std::max)(
        (std::max)(
            verticalSafeMin -
                (std::min)(
                    (std::min)(playerTopScreen_.y, playerBottomScreen_.y),
                    targetScreen_.y),
            0.0f),
        (std::max)(
            (std::max)(
                (std::max)(playerTopScreen_.y, playerBottomScreen_.y),
                targetScreen_.y) - verticalSafeMax,
            0.0f));
    const float correctionDistance_ = std::clamp(
        (playerBottomOverflow_ * 2.0f + verticalOverflow_) *
            verticalDistanceCorrectionStrength *
            (1.0f + compressionRate_),
        0.0f,
        (std::max)(maxCameraDistance - minCameraDistance, 0.0f));
    if (correctionDistance_ <= 0.001f)
    {
        return cameraPosition_;
    }

    const DirectX::XMFLOAT3 backwardDirection_ = NormalizeFloat3(
        {
            cameraPosition_.x - lookAtPosition_.x,
            cameraPosition_.y - lookAtPosition_.y,
            cameraPosition_.z - lookAtPosition_.z
        });
    return
    {
        cameraPosition_.x + backwardDirection_.x * correctionDistance_,
        cameraPosition_.y + backwardDirection_.y * correctionDistance_,
        cameraPosition_.z + backwardDirection_.z * correctionDistance_
    };
}

float CameraLockOnComponent::CalculateCollisionFov(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float compressionRate_) const
{
    float bestFov_ = baseFovY;
    float bestScore_ = (std::numeric_limits<float>::max)();
    const float maxFov_ = std::clamp(
        baseFovY + collisionFovMaxAddition * compressionRate_,
        baseFovY,
        120.0f);
    constexpr int candidateCount_ = 5;
    for (int index_ = 0; index_ < candidateCount_; ++index_)
    {
        const float rate_ =
            static_cast<float>(index_) /
            static_cast<float>(candidateCount_ - 1);
        const float fov_ = std::lerp(baseFovY, maxFov_, rate_);
        DirectX::XMFLOAT2 playerTopScreen_{};
        DirectX::XMFLOAT2 playerBottomScreen_{};
        DirectX::XMFLOAT2 targetScreen_{};
        const bool playerTopVisible_ = ProjectToScreenPosition(
            cameraPosition_,
            lookAtPosition_,
            playerTopPosition_,
            fov_,
            playerTopScreen_);
        const bool playerBottomVisible_ = ProjectToScreenPosition(
            cameraPosition_,
            lookAtPosition_,
            playerBottomPosition_,
            fov_,
            playerBottomScreen_);
        const bool targetVisible_ = ProjectToScreenPosition(
            cameraPosition_,
            lookAtPosition_,
            targetPosition_,
            fov_,
            targetScreen_);
        const float score_ = CalculateProjectionScore(
            playerTopScreen_,
            playerBottomScreen_,
            targetScreen_,
            playerTopVisible_,
            playerBottomVisible_,
            targetVisible_) +
            rate_ * 0.05f;
        if (score_ < bestScore_)
        {
            bestScore_ = score_;
            bestFov_ = fov_;
        }
    }
    return bestFov_;
}

bool CameraLockOnComponent::ProjectToScreenPosition(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& worldPosition_,
    float fovY_,
    DirectX::XMFLOAT2& screenPosition_) const
{
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_) return false;

    const DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&cameraPosition_),
        DirectX::XMLoadFloat3(&lookAtPosition_),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const DirectX::XMMATRIX projectionMatrix_ = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(fovY_),
        camera_->GetAspect(),
        camera_->GetNear(),
        camera_->GetFar());
    DirectX::XMFLOAT4 clipPosition_{};
    DirectX::XMStoreFloat4(
        &clipPosition_,
        DirectX::XMVector4Transform(
            DirectX::XMVectorSet(
                worldPosition_.x,
                worldPosition_.y,
                worldPosition_.z,
                1.0f),
            viewMatrix_ * projectionMatrix_));
    if (std::abs(clipPosition_.w) <= 0.0001f)
    {
        screenPosition_ = { 999.0f, 999.0f };
        return false;
    }
    const DirectX::XMFLOAT2 ndcPosition_
    {
        clipPosition_.x / clipPosition_.w,
        clipPosition_.y / clipPosition_.w
    };
    screenPosition_ =
    {
        (ndcPosition_.x + 1.0f) * 0.5f,
        (1.0f - ndcPosition_.y) * 0.5f
    };
    return clipPosition_.w > 0.0f;
}

float CameraLockOnComponent::CalculateProjectionScore(
    const DirectX::XMFLOAT2& playerTopScreen_,
    const DirectX::XMFLOAT2& playerBottomScreen_,
    const DirectX::XMFLOAT2& targetScreen_,
    bool playerTopVisible_,
    bool playerBottomVisible_,
    bool targetVisible_) const
{
    const float safeHorizontalMin_ = std::clamp(horizontalSafeMin, 0.0f, 0.95f);
    const float safeHorizontalMax_ = std::clamp(horizontalSafeMax, safeHorizontalMin_ + 0.01f, 1.0f);
    const float safeVerticalMin_ = std::clamp(verticalSafeMin, 0.0f, 0.95f);
    const float safeVerticalMax_ = std::clamp(verticalSafeMax, safeVerticalMin_ + 0.01f, 1.0f);
    const float safePlayerBottomLimit_ = std::clamp(
        playerBottomLimit,
        safeVerticalMin_,
        1.0f);
    auto scorePoint_ = [safeHorizontalMin_, safeHorizontalMax_,
        safeVerticalMin_, safeVerticalMax_](
            const DirectX::XMFLOAT2& screenPosition_)
    {
        const float overflowLeft_ = (std::max)(safeHorizontalMin_ - screenPosition_.x, 0.0f);
        const float overflowRight_ = (std::max)(screenPosition_.x - safeHorizontalMax_, 0.0f);
        const float overflowTop_ = (std::max)(safeVerticalMin_ - screenPosition_.y, 0.0f);
        const float overflowBottom_ = (std::max)(screenPosition_.y - safeVerticalMax_, 0.0f);
        const float centerX_ = screenPosition_.x - 0.5f;
        const float centerY_ = screenPosition_.y - 0.5f;
        return
            (overflowLeft_ * overflowLeft_ + overflowRight_ * overflowRight_) * 10.0f +
            (overflowTop_ * overflowTop_ + overflowBottom_ * overflowBottom_) * 22.0f +
            centerX_ * centerX_ * 0.25f +
            centerY_ * centerY_ * 0.45f;
    };

    const float playerBottomOverflow_ = (std::max)(
        playerBottomScreen_.y - safePlayerBottomLimit_,
        0.0f);
    const float playerScore_ =
        (std::max)(
            scorePoint_(playerTopScreen_),
            scorePoint_(playerBottomScreen_) * 1.5f) +
        playerBottomOverflow_ * playerBottomOverflow_ * 70.0f +
        (playerTopVisible_ ? 0.0f : 80.0f) +
        (playerBottomVisible_ ? 0.0f : 140.0f);
    const float targetScore_ = scorePoint_(targetScreen_) +
        (targetVisible_ ? 0.0f : 100.0f);
    const float separationScore_ =
        std::abs(playerTopScreen_.x - targetScreen_.x) * 0.05f +
        std::abs(playerTopScreen_.y - targetScreen_.y) * 0.05f;
    return (std::max)(playerScore_, targetScore_) + separationScore_;
}

CameraLockOnComponent::LockOnDistanceInfo
CameraLockOnComponent::CalculateLockOnDistanceInfo() const
{
    LockOnDistanceInfo info_{};

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> target_ = lockOnTarget.lock();

    if (!player_ || !target_ || !player_->transform || !target_->transform)
    {
        return info_;
    }

    info_.playerPosition = GetFocusPosition(player_, playerOffset);
    info_.targetPosition = GetFocusPosition(target_, targetOffset);

    info_.difference =
    {
        info_.playerPosition.x - info_.targetPosition.x,
        info_.playerPosition.y - info_.targetPosition.y,
        info_.playerPosition.z - info_.targetPosition.z
    };

    info_.horizontalDistance = std::sqrt(
        info_.difference.x * info_.difference.x +
        info_.difference.z * info_.difference.z);

    info_.targetDistance = std::sqrt(
        info_.horizontalDistance * info_.horizontalDistance +
        info_.difference.y * info_.difference.y);

    info_.isValid = true;
    return info_;
}

void CameraLockOnComponent::DrawLockOnDebugGui()
{
    ImGui::SeparatorText("Debug");

    ResolveReferences();

    const LockOnDistanceInfo distanceInfo_ = CalculateLockOnDistanceInfo();

    if (!distanceInfo_.isValid)
    {
        ImGui::TextDisabled("LockOn Distance: invalid target");
        return;
    }

    ImGui::Text("Target Distance: %.3f", distanceInfo_.targetDistance);
    ImGui::Text("Horizontal Distance: %.3f", distanceInfo_.horizontalDistance);
    ImGui::Text("Height Difference: %.3f", distanceInfo_.difference.y);
    ImGui::Text("Max Distance: %.3f", maxDistance);

    ImGui::Text(
        "Player Focus: %.3f, %.3f, %.3f",
        distanceInfo_.playerPosition.x,
        distanceInfo_.playerPosition.y,
        distanceInfo_.playerPosition.z);

    ImGui::Text(
        "Target Focus: %.3f, %.3f, %.3f",
        distanceInfo_.targetPosition.x,
        distanceInfo_.targetPosition.y,
        distanceInfo_.targetPosition.z);

    if (distanceInfo_.targetDistance > maxDistance)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
            "Out of LockOn Range");
    }
    else
    {
        ImGui::TextColored(
            ImVec4(0.25f, 1.0f, 0.25f, 1.0f),
            "In LockOn Range");
    }
}

void CameraLockOnComponent::Gui()
{
    ImGui::Checkbox("Lock On Enabled", &lockOnEnabled);

    char playerBuffer_[128]{};
    strcpy_s(playerBuffer_, playerTargetName.c_str());
    if (ImGui::InputText("Player Target", playerBuffer_, sizeof(playerBuffer_)))
    {
        playerTargetName = playerBuffer_;
        playerTarget.reset();
        ResolveReferences();
        ResetTracking();
    }
    char targetBuffer_[128]{};
    strcpy_s(targetBuffer_, lockOnTargetName.c_str());
    if (ImGui::InputText("Lock Target", targetBuffer_, sizeof(targetBuffer_)))
    {
        lockOnTargetName = targetBuffer_;
        lockOnTarget.reset();
        ResolveReferences();
        ResetTracking();
    }

    ImGui::DragFloat3("Player Offset", &playerOffset.x, 0.05f);
    ImGui::DragFloat3("Player Bottom Offset", &playerBottomOffset.x, 0.05f);
    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.05f);

    DrawLockOnDebugGui();


    ImGui::DragFloat("Max Distance", &maxDistance, 0.1f, 0.1f, 200.0f);
    ImGui::DragFloat("Base Distance", &baseDistance, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Distance Scale", &distanceScale, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloatRange2(
        "Camera Distance Range",
        &minCameraDistance,
        &maxCameraDistance,
        0.1f,
        0.1f,
        100.0f);
    ImGui::DragFloat("Camera Height", &cameraHeight, 0.05f, -20.0f, 50.0f);
    ImGui::DragFloat("Separation Height", &separationHeightScale, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Height Difference", &heightDifferenceScale, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Side Offset", &sideOffset, 0.05f, 0.0f, 20.0f);
    if (ImGui::RadioButton("Right Shoulder", currentSide > 0.0f)) currentSide = 1.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("Left Shoulder", currentSide < 0.0f)) currentSide = -1.0f;
    ImGui::DragFloat("Close Distance", &closeDistance, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Close Focus Bias", &closeFocusBias, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Far Focus Bias", &farFocusBias, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Position Speed", &positionInterpolationSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("LookAt Speed", &lookAtInterpolationSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::SeparatorText("Collision Composition");
    ImGui::DragFloat("horizontalSafeMin", &horizontalSafeMin, 0.01f, 0.0f, 0.95f);
    ImGui::DragFloat("horizontalSafeMax", &horizontalSafeMax, 0.01f, 0.05f, 1.0f);
    ImGui::DragFloat("verticalSafeMin", &verticalSafeMin, 0.01f, 0.0f, 0.95f);
    ImGui::DragFloat("verticalSafeMax", &verticalSafeMax, 0.01f, 0.05f, 1.0f);
    ImGui::DragFloat("playerBottomLimit", &playerBottomLimit, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("playerFocusWeight", &playerFocusWeight, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("targetFocusWeight", &targetFocusWeight, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat(
        "verticalDistanceCorrectionStrength",
        &verticalDistanceCorrectionStrength,
        0.05f,
        0.0f,
        20.0f);
    ImGui::DragFloat(
        "verticalFocusCorrectionStrength",
        &verticalFocusCorrectionStrength,
        0.05f,
        0.0f,
        10.0f);
    ImGui::DragFloat("framingSmoothSpeed", &framingSmoothSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Collision LookAt Lower Height", &collisionLookAtLowerHeight, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Collision LookAt Search Radius", &collisionLookAtSearchRadius, 0.05f, 0.0f, 8.0f);
    ImGui::DragFloat("Collision LookAt Speed", &collisionLookAtInterpolationSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Collision FOV Max Addition", &collisionFovMaxAddition, 0.5f, 0.0f, 60.0f);
    ImGui::DragFloat("Collision FOV Speed", &collisionFovInterpolationSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::Text(
        "Player Screen: %.3f, %.3f",
        debugPlayerScreen.x,
        debugPlayerScreen.y);
    ImGui::Text(
        "Player Bottom Screen: %.3f, %.3f",
        debugPlayerBottomScreen.x,
        debugPlayerBottomScreen.y);
    ImGui::Text(
        "Target Screen: %.3f, %.3f",
        debugTargetScreen.x,
        debugTargetScreen.y);
    ImGui::Text(
        "Safe X: %.2f - %.2f / Safe Y: %.2f - %.2f / Player Bottom: %.2f",
        horizontalSafeMin,
        horizontalSafeMax,
        verticalSafeMin,
        verticalSafeMax,
        playerBottomLimit);
    ImGui::Text(
        "Correction H:%s V:%s PlayerBottom:%s FOV:%s",
        debugHorizontalCorrection ? "ON" : "OFF",
        debugVerticalCorrection ? "ON" : "OFF",
        debugPlayerBottomCorrection ? "ON" : "OFF",
        debugFovCorrection ? "ON" : "OFF");
    ImGui::TextDisabled("Actions: LockOn_Enable / Disable / Toggle / SwitchSide");
}

void CameraLockOnComponent::Serialize(nlohmann::json& json_) const
{
    json_["playerTargetName"] = playerTargetName;
    json_["lockOnTargetName"] = lockOnTargetName;
    json_["lockOnEnabled"] = lockOnEnabled;
    json_["playerOffset"] = { playerOffset.x, playerOffset.y, playerOffset.z };
    json_["playerBottomOffset"] =
    {
        playerBottomOffset.x,
        playerBottomOffset.y,
        playerBottomOffset.z
    };
    json_["targetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
    json_["maxDistance"] = maxDistance;
    json_["baseDistance"] = baseDistance;
    json_["distanceScale"] = distanceScale;
    json_["minCameraDistance"] = minCameraDistance;
    json_["maxCameraDistance"] = maxCameraDistance;
    json_["cameraHeight"] = cameraHeight;
    json_["separationHeightScale"] = separationHeightScale;
    json_["heightDifferenceScale"] = heightDifferenceScale;
    json_["sideOffset"] = sideOffset;
    json_["currentSide"] = currentSide;
    json_["closeDistance"] = closeDistance;
    json_["closeFocusBias"] = closeFocusBias;
    json_["farFocusBias"] = farFocusBias;
    json_["positionInterpolationSpeed"] = positionInterpolationSpeed;
    json_["lookAtInterpolationSpeed"] = lookAtInterpolationSpeed;
    json_["horizontalSafeMin"] = horizontalSafeMin;
    json_["horizontalSafeMax"] = horizontalSafeMax;
    json_["verticalSafeMin"] = verticalSafeMin;
    json_["verticalSafeMax"] = verticalSafeMax;
    json_["playerBottomLimit"] = playerBottomLimit;
    json_["playerFocusWeight"] = playerFocusWeight;
    json_["targetFocusWeight"] = targetFocusWeight;
    json_["verticalDistanceCorrectionStrength"] =
        verticalDistanceCorrectionStrength;
    json_["verticalFocusCorrectionStrength"] =
        verticalFocusCorrectionStrength;
    json_["framingSmoothSpeed"] = framingSmoothSpeed;
    json_["collisionLookAtLowerHeight"] = collisionLookAtLowerHeight;
    json_["collisionLookAtSearchRadius"] = collisionLookAtSearchRadius;
    json_["collisionLookAtInterpolationSpeed"] =
        collisionLookAtInterpolationSpeed;
    json_["collisionFovMaxAddition"] = collisionFovMaxAddition;
    json_["collisionFovInterpolationSpeed"] =
        collisionFovInterpolationSpeed;
}

void CameraLockOnComponent::Deserialize(const nlohmann::json& json_)
{
    playerTargetName = json_.value("playerTargetName", std::string{});
    lockOnTargetName = json_.value("lockOnTargetName", std::string{});
    lockOnEnabled = json_.value("lockOnEnabled", false);
    auto loadFloat3_ = [&json_](
        const char* key_,
        DirectX::XMFLOAT3& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() ||
            json_[key_].size() < 3) return;
        value_ =
        {
            json_[key_][0].get<float>(),
            json_[key_][1].get<float>(),
            json_[key_][2].get<float>()
        };
    };
    loadFloat3_("playerOffset", playerOffset);
    loadFloat3_("playerBottomOffset", playerBottomOffset);
    loadFloat3_("targetOffset", targetOffset);
    maxDistance = json_.value("maxDistance", 28.0f);
    baseDistance = json_.value("baseDistance", 7.0f);
    distanceScale = json_.value("distanceScale", 0.45f);
    minCameraDistance = json_.value("minCameraDistance", 6.0f);
    maxCameraDistance = json_.value("maxCameraDistance", 16.0f);
    cameraHeight = json_.value("cameraHeight", 2.2f);
    separationHeightScale = json_.value("separationHeightScale", 0.08f);
    heightDifferenceScale = json_.value("heightDifferenceScale", 0.35f);
    sideOffset = json_.value("sideOffset", 1.25f);
    currentSide = json_.value("currentSide", 1.0f) >= 0.0f ? 1.0f : -1.0f;
    closeDistance = json_.value("closeDistance", 8.0f);
    closeFocusBias = json_.value("closeFocusBias", 0.28f);
    farFocusBias = json_.value("farFocusBias", 0.5f);
    positionInterpolationSpeed = json_.value("positionInterpolationSpeed", 10.0f);
    lookAtInterpolationSpeed = json_.value("lookAtInterpolationSpeed", 12.5f);
    horizontalSafeMin = json_.value("horizontalSafeMin", 0.10f);
    horizontalSafeMax = json_.value("horizontalSafeMax", 0.90f);
    verticalSafeMin = json_.value("verticalSafeMin", 0.22f);
    verticalSafeMax = json_.value("verticalSafeMax", 0.78f);
    playerBottomLimit = json_.value("playerBottomLimit", 0.70f);
    playerFocusWeight = json_.value("playerFocusWeight", 0.62f);
    targetFocusWeight = json_.value("targetFocusWeight", 0.38f);
    verticalDistanceCorrectionStrength =
        json_.value("verticalDistanceCorrectionStrength", 2.5f);
    verticalFocusCorrectionStrength =
        json_.value("verticalFocusCorrectionStrength", 1.4f);
    framingSmoothSpeed = json_.value("framingSmoothSpeed", 16.0f);
    collisionLookAtLowerHeight =
        json_.value("collisionLookAtLowerHeight", 1.1f);
    collisionLookAtSearchRadius =
        json_.value("collisionLookAtSearchRadius", 1.8f);
    collisionLookAtInterpolationSpeed =
        json_.value("collisionLookAtInterpolationSpeed", 18.0f);
    collisionFovMaxAddition =
        json_.value("collisionFovMaxAddition", 18.0f);
    collisionFovInterpolationSpeed =
        json_.value("collisionFovInterpolationSpeed", 10.0f);

    horizontalSafeMin = std::clamp(horizontalSafeMin, 0.0f, 0.95f);
    horizontalSafeMax = std::clamp(horizontalSafeMax, horizontalSafeMin + 0.01f, 1.0f);
    verticalSafeMin = std::clamp(verticalSafeMin, 0.0f, 0.95f);
    verticalSafeMax = std::clamp(verticalSafeMax, verticalSafeMin + 0.01f, 1.0f);
    playerBottomLimit = std::clamp(playerBottomLimit, verticalSafeMin, 1.0f);
    playerFocusWeight = std::clamp(playerFocusWeight, 0.0f, 1.0f);
    targetFocusWeight = std::clamp(targetFocusWeight, 0.0f, 1.0f);
    verticalDistanceCorrectionStrength =
        std::clamp(verticalDistanceCorrectionStrength, 0.0f, 20.0f);
    verticalFocusCorrectionStrength =
        std::clamp(verticalFocusCorrectionStrength, 0.0f, 10.0f);
    framingSmoothSpeed = std::clamp(framingSmoothSpeed, 0.1f, 100.0f);
    collisionLookAtLowerHeight =
        std::clamp(collisionLookAtLowerHeight, 0.0f, 5.0f);
    collisionLookAtSearchRadius =
        std::clamp(collisionLookAtSearchRadius, 0.0f, 8.0f);
    collisionLookAtInterpolationSpeed =
        std::clamp(collisionLookAtInterpolationSpeed, 0.1f, 100.0f);
    collisionFovMaxAddition =
        std::clamp(collisionFovMaxAddition, 0.0f, 60.0f);
    collisionFovInterpolationSpeed =
        std::clamp(collisionFovInterpolationSpeed, 0.1f, 100.0f);
    ResetTracking();
}

REGISTER_COMPONENT(CameraLockOnComponent, "CameraLockOnComponent")
