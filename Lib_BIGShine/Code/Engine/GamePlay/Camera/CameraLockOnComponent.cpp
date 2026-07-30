#include "PrecompiledHeader.h"

#include "CameraLockOnComponent.h"

#include "CameraComponent.h"
#include "CameraManager.h"
#include "LockOnTargetComponent.h"
#include "ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
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

    DirectX::XMFLOAT3 SubtractFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x - right_.x, left_.y - right_.y, left_.z - right_.z };
    }

    DirectX::XMFLOAT3 ScaleFloat3(
        const DirectX::XMFLOAT3& value_,
        float scale_)
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }

    float LengthHorizontal(
        const DirectX::XMFLOAT3& value_)
    {
        return std::sqrt(value_.x * value_.x + value_.z * value_.z);
    }

    DirectX::XMFLOAT3 NormalizeHorizontal(
        const DirectX::XMFLOAT3& value_,
        const DirectX::XMFLOAT3& fallback_)
    {
        const float length_ = LengthHorizontal(value_);
        if (length_ <= 0.0001f)
        {
            const float fallbackLength_ = LengthHorizontal(fallback_);
            if (fallbackLength_ <= 0.0001f)
            {
                return { 0.0f, 0.0f, 1.0f };
            }
            return
            {
                fallback_.x / fallbackLength_,
                0.0f,
                fallback_.z / fallbackLength_
            };
        }
        return
        {
            value_.x / length_,
            0.0f,
            value_.z / length_
        };
    }

    float SmoothStep(float rate_)
    {
        const float clampedRate_ = std::clamp(rate_, 0.0f, 1.0f);
        return clampedRate_ * clampedRate_ * (3.0f - 2.0f * clampedRate_);
    }

    float ExpSmoothingRate(float speed_, float deltaTime_)
    {
        return 1.0f - std::exp(-(std::max)(speed_, 0.0f) * (std::max)(deltaTime_, 0.0f));
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
    BS_PROFILE_SCOPE("CameraLockOnComponent::Start");
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
    BS_PROFILE_SCOPE("CameraLockOnComponent::LateUpdate");
    ResolveReferences();
    if (actionSuppressed) return;
    if (switchCooldownTimer > 0.0f)
    {
        switchCooldownTimer = (std::max)(switchCooldownTimer - deltaTime_, 0.0f);
    }
    if (!lockOnEnabled)
    {
        accumulatedMouseDelta = { 0.0f, 0.0f };
        debugMouseAccumulatedDelta = accumulatedMouseDelta;
        switchInputLatched = false;
        ReleaseThirdPersonCamera();
        return;
    }

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> enemy_ = lockOnTarget.lock();
    if (!ResolveFocusTargetFallback())
    {
        SetLockOnEnabled(false);
        return;
    }
    UpdateLockOnSwitchInput(deltaTime_);
    const std::shared_ptr<GameObject> focusTarget_ = lockOnFocusTarget.lock();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
        thirdPersonCamera.lock();
    if (!player_ || !enemy_ || !focusTarget_ || !camera_ ||
        !player_->transform || !enemy_->transform || !focusTarget_->transform)
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
    SynchronizeExternalFovChange(camera_);

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
    currentTargetDistance = targetDistance_;

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
    DirectX::XMFLOAT3 desiredPosition_
    {
        playerPosition_.x + backDirection_.x * desiredDistance_ +
            sideDirection_.x * sideOffset * currentSide,
        playerPosition_.y + desiredHeight_,
        playerPosition_.z + backDirection_.z * desiredDistance_ +
            sideDirection_.z * sideOffset * currentSide
    };
    debugNormalCameraPosition = desiredPosition_;

    const float focusRate_ = std::clamp(
        horizontalDistance_ / (std::max)(closeDistance, 0.001f),
        0.0f,
        1.0f);
    const float enemyFocusWeight_ = std::lerp(
        closeFocusBias,
        farFocusBias,
        focusRate_);
    DirectX::XMFLOAT3 desiredLookAt_ = LerpFloat3(
        playerPosition_,
        enemyPosition_,
        enemyFocusWeight_);
    currentBaseCameraPosition = desiredPosition_;
    currentBaseLookAtPosition = desiredLookAt_;

    const float compositionWeight_ =
        std::clamp(compositionCorrection.weight, 0.0f, 1.0f);
    if (compositionWeight_ > 0.0001f)
    {
        desiredPosition_ = AddFloat3(
            desiredPosition_,
            ScaleFloat3(
                backDirection_,
                compositionCorrection.distanceOffset * compositionWeight_));
        desiredPosition_.y +=
            compositionCorrection.heightOffset * compositionWeight_;
        desiredLookAt_.y +=
            compositionCorrection.lookAtHeightOffset * compositionWeight_;
        desiredLookAt_ = AddFloat3(
            desiredLookAt_,
            ScaleFloat3(
                SubtractFloat3(enemyPosition_, playerPosition_),
                compositionCorrection.focusTargetRateOffset * compositionWeight_));
    }
    DirectX::XMFLOAT3 finalDesiredPosition_ = desiredPosition_;
    DirectX::XMFLOAT3 finalDesiredLookAt_ = desiredLookAt_;
    float wallFovAddition_ =
        compositionCorrection.fovOffset * compositionWeight_;

    debugDistanceToWall = arenaRadius;
    debugRawWallCameraRate = 0.0f;
    debugLeftCandidateScore = 0.0f;
    debugCenterCandidateScore = 0.0f;
    debugRightCandidateScore = 0.0f;
    debugSelectedCandidateScore = 0.0f;
    debugSelectedWallCameraPosition = ClampCameraPositionToArena(desiredPosition_);
    debugFinalDesiredCameraPosition = finalDesiredPosition_;
    const float arenaDirectionDeadZone_ =
        (std::max)(1.0f, (std::max)(arenaCameraMargin, 0.0f));
    DirectX::XMFLOAT3 centerToPlayer_{ 0.0f, 0.0f, 0.0f };
    float playerArenaRadius_ = 0.0f;
    bool hasStableArenaDirection_ = false;
    const bool canUseCircularArenaCamera_ =
        enableCircularArenaCamera &&
        arenaRadius > 0.001f &&
        arenaRadius > arenaCameraMargin + 0.001f &&
        wallCameraStartDistance > 0.001f;
    bool isPlayerInsideArenaRange_ = false;
    if (canUseCircularArenaCamera_)
    {
        centerToPlayer_ = SubtractFloat3(playerPosition_, arenaCenter);
        playerArenaRadius_ = LengthHorizontal(centerToPlayer_);
        hasStableArenaDirection_ = playerArenaRadius_ > arenaDirectionDeadZone_;
        debugDistanceToWall = arenaRadius - playerArenaRadius_;
        isPlayerInsideArenaRange_ =
            playerArenaRadius_ <=
            arenaRadius + (std::max)(wallCameraStartDistance, arenaCameraMargin);
        debugRawWallCameraRate = std::clamp(
            1.0f - debugDistanceToWall /
                (std::max)(wallCameraStartDistance, 0.001f),
            0.0f,
            1.0f);
    }

    if (canUseCircularArenaCamera_ && isPlayerInsideArenaRange_ && hasStableArenaDirection_)
    {
        currentWallCameraRate = std::lerp(
            currentWallCameraRate,
            SmoothStep(debugRawWallCameraRate),
            ExpSmoothingRate(wallCameraRateSmoothSpeed, deltaTime_));
    }
    else
    {
        currentWallCameraRate = std::lerp(
            currentWallCameraRate,
            0.0f,
            ExpSmoothingRate(wallCameraRateSmoothSpeed, deltaTime_));
    }

    if (currentWallCameraRate > 0.001f && hasStableArenaDirection_)
    {
        // 壁際では外周バリアへ押し込まれないよう、カメラ候補をステージ中心側へ寄せる。
        const DirectX::XMFLOAT3 normalCameraDirection_ = NormalizeHorizontal(
            SubtractFloat3(desiredPosition_, playerPosition_),
            backDirection_);
        const DirectX::XMFLOAT3 outwardDirection_ = NormalizeHorizontal(
            centerToPlayer_,
            hasArenaOutwardDirection
                ? previousArenaOutwardDirection
                : ScaleFloat3(normalCameraDirection_, -1.0f));
        previousArenaOutwardDirection = outwardDirection_;
        hasArenaOutwardDirection = true;
        const DirectX::XMFLOAT3 inwardDirection_
        {
            -outwardDirection_.x,
            0.0f,
            -outwardDirection_.z
        };
        const DirectX::XMFLOAT3 tangentDirection_
        {
            -outwardDirection_.z,
            0.0f,
            outwardDirection_.x
        };
        const float directionWeightTotal_ = (std::max)(
            (std::max)(wallCenterWeight, 0.0f) +
                (std::max)(wallCombatWeight, 0.0f),
            0.001f);
        const DirectX::XMFLOAT3 wallBaseDirection_ = NormalizeHorizontal(
            AddFloat3(
                ScaleFloat3(inwardDirection_, (std::max)(wallCenterWeight, 0.0f) / directionWeightTotal_),
                ScaleFloat3(normalCameraDirection_, (std::max)(wallCombatWeight, 0.0f) / directionWeightTotal_)),
            inwardDirection_);
        const DirectX::XMFLOAT3 wallBasePosition_
        {
            playerPosition_.x + wallBaseDirection_.x * wallCameraDistance,
            playerPosition_.y + wallCameraHeight +
                wallCameraMaximumHeightAddition * currentWallCameraRate,
            playerPosition_.z + wallBaseDirection_.z * wallCameraDistance
        };
        const DirectX::XMFLOAT3 centerCandidate_ =
            ClampCameraPositionToArena(wallBasePosition_);
        const DirectX::XMFLOAT3 leftCandidate_ =
            ClampCameraPositionToArena(AddFloat3(
                wallBasePosition_,
                ScaleFloat3(tangentDirection_, wallCameraSideOffset)));
        const DirectX::XMFLOAT3 rightCandidate_ =
            ClampCameraPositionToArena(SubtractFloat3(
                wallBasePosition_,
                ScaleFloat3(tangentDirection_, wallCameraSideOffset)));
        const DirectX::XMFLOAT3 playerBottomPosition_ =
            GetPlayerBottomPosition(player_);
        const float wallCandidateFov_ = std::clamp(
            baseFovY + wallCameraMaximumFovAddition * currentWallCameraRate,
            20.0f,
            120.0f);
        finalDesiredLookAt_ = LerpFloat3(
            desiredLookAt_,
            playerPosition_,
            std::clamp(
                wallLookAtPlayerBias * SmoothStep(currentWallCameraRate),
                0.0f,
                0.85f));

        // 左右候補を画面内収まりで評価し、プレイヤーの足元と敵が残る位置を優先する。
        debugLeftCandidateScore = CalculateWallCameraCandidateScore(
            leftCandidate_,
            finalDesiredLookAt_,
            playerPosition_,
            playerBottomPosition_,
            enemyPosition_,
            wallCandidateFov_,
            -1);
        debugCenterCandidateScore = CalculateWallCameraCandidateScore(
            centerCandidate_,
            finalDesiredLookAt_,
            playerPosition_,
            playerBottomPosition_,
            enemyPosition_,
            wallCandidateFov_,
            0);
        debugRightCandidateScore = CalculateWallCameraCandidateScore(
            rightCandidate_,
            finalDesiredLookAt_,
            playerPosition_,
            playerBottomPosition_,
            enemyPosition_,
            wallCandidateFov_,
            1);

        if (hasWallCameraPosition)
        {
            const float continuityWeight_ =
                (std::max)(wallCameraContinuityScoreWeight, 0.0f);
            debugLeftCandidateScore +=
                LengthHorizontal(SubtractFloat3(leftCandidate_, previousWallCameraPosition)) *
                continuityWeight_;
            debugCenterCandidateScore +=
                LengthHorizontal(SubtractFloat3(centerCandidate_, previousWallCameraPosition)) *
                continuityWeight_;
            debugRightCandidateScore +=
                LengthHorizontal(SubtractFloat3(rightCandidate_, previousWallCameraPosition)) *
                continuityWeight_;
        }

        int bestSide_ = 0;
        float bestScore_ = debugCenterCandidateScore;
        if (debugLeftCandidateScore < bestScore_)
        {
            bestSide_ = -1;
            bestScore_ = debugLeftCandidateScore;
        }
        if (debugRightCandidateScore < bestScore_)
        {
            bestSide_ = 1;
            bestScore_ = debugRightCandidateScore;
        }

        wallCameraSideHoldTimer += (std::max)(deltaTime_, 0.0f);
        auto getCandidateScore_ = [this](int side_)
        {
            if (side_ < 0) return debugLeftCandidateScore;
            if (side_ > 0) return debugRightCandidateScore;
            return debugCenterCandidateScore;
        };
        // 左右の頻繁な切り替わりを避けるため、一定時間とスコア差を満たすまで保持する。
        if (currentWallCameraSide == 0 && currentWallCameraRate > 0.25f)
        {
            currentWallCameraSide = bestSide_;
            wallCameraSideHoldTimer = 0.0f;
        }
        else if (bestSide_ != currentWallCameraSide)
        {
            const bool isOppositeSideSwitch_ =
                currentWallCameraSide != 0 &&
                bestSide_ != 0 &&
                bestSide_ != currentWallCameraSide;
            const float requiredHoldTime_ = isOppositeSideSwitch_
                ? (std::max)(wallCameraOppositeSideMinimumHoldTime, wallCameraSideMinimumHoldTime)
                : wallCameraSideMinimumHoldTime;
            const float requiredScoreThreshold_ = isOppositeSideSwitch_
                ? (std::max)(wallCameraOppositeSideSwitchScoreThreshold, wallCameraSideSwitchScoreThreshold)
                : wallCameraSideSwitchScoreThreshold;
            if (wallCameraSideHoldTimer >= requiredHoldTime_ &&
                bestScore_ + requiredScoreThreshold_ <
                    getCandidateScore_(currentWallCameraSide))
            {
                currentWallCameraSide = bestSide_;
                wallCameraSideHoldTimer = 0.0f;
            }
        }

        DirectX::XMFLOAT3 selectedWallCameraPosition_ = centerCandidate_;
        debugSelectedCandidateScore = debugCenterCandidateScore;
        if (currentWallCameraSide < 0)
        {
            selectedWallCameraPosition_ = leftCandidate_;
            debugSelectedCandidateScore = debugLeftCandidateScore;
        }
        else if (currentWallCameraSide > 0)
        {
            selectedWallCameraPosition_ = rightCandidate_;
            debugSelectedCandidateScore = debugRightCandidateScore;
        }
        debugSelectedWallCameraPosition = selectedWallCameraPosition_;
        previousWallCameraPosition = selectedWallCameraPosition_;
        hasWallCameraPosition = true;
        finalDesiredPosition_ = LerpFloat3(
            desiredPosition_,
            selectedWallCameraPosition_,
            SmoothStep(currentWallCameraRate));
        finalDesiredPosition_ = ClampCameraPositionToArena(finalDesiredPosition_);
        wallFovAddition_ =
            wallCameraMaximumFovAddition * SmoothStep(currentWallCameraRate);
    }
    else if (debugRawWallCameraRate <= 0.001f)
    {
        wallCameraSideHoldTimer = 0.0f;
        if (currentWallCameraRate <= 0.001f)
        {
            currentWallCameraSide = 0;
            hasWallCameraPosition = false;
        }
    }
    if (compositionWeight_ > 0.0001f)
    {
        const float sideAllowRate_ = std::clamp(
            1.0f - currentWallCameraRate * 0.8f,
            0.0f,
            1.0f);
        finalDesiredPosition_ = AddFloat3(
            finalDesiredPosition_,
            ScaleFloat3(
                sideDirection_,
                compositionCorrection.sideOffset *
                    compositionWeight_ *
                    sideAllowRate_));
        finalDesiredPosition_ = ClampCameraPositionToArena(finalDesiredPosition_);
    }
    debugFinalDesiredCameraPosition = finalDesiredPosition_;
    debugCameraArenaRadius = LengthHorizontal(
        SubtractFloat3(finalDesiredPosition_, arenaCenter));
    debugMaximumCameraArenaRadius = (std::max)(
        arenaRadius - (std::max)(arenaCameraMargin, 0.0f),
        0.001f);

    const std::shared_ptr<GameObject> previousTarget_ =
        previousLockOnTarget.lock();
    const std::shared_ptr<GameObject> previousFocusTarget_ =
        previousFocusTarget.lock();
    if (!hasTrackingPose || previousTarget_ != enemy_)
    {
        interpolatedPosition = camera_->GetEye();
        interpolatedLookAt = camera_->GetFocus();
        previousLockOnTarget = enemy_;
        previousFocusTarget = focusTarget_;
        hasTrackingPose = true;
    }
    else if (previousFocusTarget_ != focusTarget_)
    {
        previousFocusTarget = focusTarget_;
    }

    interpolatedPosition = LerpFloat3(
        interpolatedPosition,
        finalDesiredPosition_,
        ExpSmoothingRate(
            (std::max)(positionInterpolationSpeed, wallCameraPositionSmoothSpeed),
            deltaTime_));
    interpolatedLookAt = LerpFloat3(
        interpolatedLookAt,
        finalDesiredLookAt_,
        ExpSmoothingRate(lookAtInterpolationSpeed, deltaTime_));
    if (!hasCollisionComposition)
    {
        collisionLookAt = interpolatedLookAt;
    }
    currentCollisionFov = std::lerp(
        currentCollisionFov,
        std::clamp(baseFovY + wallFovAddition_, 20.0f, 120.0f),
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
    if (enabled_) ResolveFocusTargetFallback();
    lockOnEnabled = enabled_ && !lockOnTarget.expired() && !lockOnFocusTarget.expired();
    ResetTracking();
    if (!lockOnEnabled)
    {
        ClearCompositionCorrection();
        lockOnFocusTarget.reset();
        lockOnFocusTargetName.clear();
        accumulatedMouseDelta = { 0.0f, 0.0f };
        debugMouseAccumulatedDelta = accumulatedMouseDelta;
        switchInputLatched = false;
        switchCooldownTimer = 0.0f;
        if (const std::shared_ptr<CameraComponent> camera_ = camera.lock();
            camera_ && hasBaseFov)
        {
            const float cameraFov_ = camera_->GetFovY();
            if (std::abs(cameraFov_ - currentCollisionFov) <= 0.05f)
            {
                currentCollisionFov = baseFovY;
                camera_->SetFov(baseFovY);
            }
            else
            {
                baseFovY = cameraFov_;
                currentCollisionFov = cameraFov_;
            }
        }
        ReleaseThirdPersonCamera();
    }
}

void CameraLockOnComponent::SetActionSuppressed(bool suppressed_)
{
    actionSuppressed = suppressed_;
    if (!actionSuppressed) ResetTracking();
}

void CameraLockOnComponent::SetCompositionCorrection(
    const LockOnCameraCompositionCorrection& correction_)
{
    compositionCorrection = correction_;
    compositionCorrection.weight =
        std::clamp(compositionCorrection.weight, 0.0f, 1.0f);
}

void CameraLockOnComponent::ClearCompositionCorrection()
{
    compositionCorrection = {};
}

bool CameraLockOnComponent::IsWallCameraActive() const
{
    return lockOnEnabled &&
        enableCircularArenaCamera &&
        currentWallCameraRate > 0.05f;
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
    lockOnFocusTarget.reset();
    lockOnFocusTargetName.clear();
    ResolveFocusTargetFallback();
    ResetTracking();
}

void CameraLockOnComponent::SetLockOnFocusTarget(
    const std::shared_ptr<GameObject>& focusTarget_)
{
    if (!focusTarget_)
    {
        lockOnFocusTarget.reset();
        lockOnFocusTargetName.clear();
        ResolveFocusTargetFallback();
        return;
    }

    lockOnFocusTarget = focusTarget_;
    lockOnFocusTargetName = focusTarget_->GetName();
    if (const std::shared_ptr<LockOnTargetComponent> lockTarget_ =
        focusTarget_->GetComponent<LockOnTargetComponent>())
    {
        const std::shared_ptr<GameObject> combatOwner_ =
            lockTarget_->GetCombatOwner();
        if (combatOwner_)
        {
            lockOnTarget = combatOwner_;
            lockOnTargetName = combatOwner_->GetName();
        }
    }
    else
    {
        lockOnTarget = focusTarget_;
        lockOnTargetName = focusTarget_->GetName();
    }
}

std::shared_ptr<GameObject> CameraLockOnComponent::FindBestLockOnTarget(
    const std::shared_ptr<GameObject>& playerTarget_) const
{
    return FindBestLockOnCandidate(playerTarget_).combatOwner.lock();
}

CameraLockOnComponent::LockOnCandidate CameraLockOnComponent::FindBestLockOnCandidate(
    const std::shared_ptr<GameObject>& playerTarget_) const
{
    LockOnCandidate bestCandidate_{};
    float bestScore_ = (std::numeric_limits<float>::max)();
    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget_);
    for (LockOnCandidate candidate_ : candidates_)
    {
        const float centerX_ = candidate_.screenPosition.x - 0.5f;
        const float centerY_ = candidate_.screenPosition.y - 0.5f;
        const float centerDistance_ =
            std::sqrt(centerX_ * centerX_ + centerY_ * centerY_);
        candidate_.score =
            centerDistance_ * 8.0f +
            candidate_.distance / (std::max)(maxDistance, 0.001f) +
            (candidate_.isVisible ? 0.0f : 40.0f) -
            candidate_.priority * priorityWeight -
            (candidate_.isDefaultTarget ? 3.0f : 0.0f) +
            (candidate_.isCombatOwnerSelf ? 0.02f : 0.0f);

        if (candidate_.score < bestScore_)
        {
            bestScore_ = candidate_.score;
            bestCandidate_ = candidate_;
        }
    }

    debugCandidateCount = static_cast<int>(candidates_.size());
    debugLastSwitchScore = bestScore_ == (std::numeric_limits<float>::max)()
        ? 0.0f
        : bestScore_;
    return bestCandidate_;
}

std::vector<CameraLockOnComponent::LockOnCandidate>
CameraLockOnComponent::CollectLockOnCandidates(
    const std::shared_ptr<GameObject>& playerTarget_) const
{
    std::vector<LockOnCandidate> candidates_;
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager || !playerTarget_ || !playerTarget_->transform)
    {
        return candidates_;
    }

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_) return candidates_;

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
    else
    {
        cameraForward_ = camera_->GetFront();
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

    for (const std::shared_ptr<GameObject>& candidate_ : owner_->manager->GetObjects())
    {
        if (!candidate_ || candidate_ == playerTarget_ || !candidate_->transform)
        {
            continue;
        }
        const std::shared_ptr<LockOnTargetComponent> lockTarget_ =
            candidate_->GetComponent<LockOnTargetComponent>();
        if (!lockTarget_ || !lockTarget_->IsLockOnEnabled()) continue;

        const std::shared_ptr<GameObject> combatOwner_ =
            lockTarget_->GetCombatOwner();
        if (!combatOwner_ || !combatOwner_->transform) continue;

        const DirectX::XMFLOAT3 targetPosition_ = lockTarget_->GetFocusPosition();
        const float differenceX_ = targetPosition_.x - playerPosition_.x;
        const float differenceY_ = targetPosition_.y - playerPosition_.y;
        const float differenceZ_ = targetPosition_.z - playerPosition_.z;
        const float horizontalDistance_ = std::sqrt(
            differenceX_ * differenceX_ + differenceZ_ * differenceZ_);
        const float targetDistance_ = std::sqrt(
            horizontalDistance_ * horizontalDistance_ + differenceY_ * differenceY_);
        if (targetDistance_ <= 0.0001f || targetDistance_ > maxDistance)
        {
            continue;
        }

        const float directionX_ = differenceX_ / horizontalDistance_;
        const float directionZ_ = differenceZ_ / horizontalDistance_;
        const float centerDot_ = std::clamp(
            cameraForward_.x * directionX_ + cameraForward_.z * directionZ_,
            -1.0f,
            1.0f);
        if (centerDot_ <= 0.0f) continue;

        LockOnCandidate lockOnCandidate_{};
        lockOnCandidate_.focusTarget = candidate_;
        lockOnCandidate_.combatOwner = combatOwner_;
        lockOnCandidate_.worldPosition = targetPosition_;
        lockOnCandidate_.distance = targetDistance_;
        lockOnCandidate_.priority = lockTarget_->GetPriority();
        lockOnCandidate_.isDefaultTarget = lockTarget_->IsDefaultTarget();
        lockOnCandidate_.isCombatOwnerSelf = candidate_ == combatOwner_;
        lockOnCandidate_.isVisible = ProjectToScreenPosition(
            camera_->GetEye(),
            camera_->GetFocus(),
            targetPosition_,
            camera_->GetFovY(),
            lockOnCandidate_.screenPosition);

        if (!lockOnCandidate_.isVisible)
        {
            const float overflowX_ =
                (std::max)(-lockOnCandidate_.screenPosition.x, 0.0f) +
                (std::max)(lockOnCandidate_.screenPosition.x - 1.0f, 0.0f);
            const float overflowY_ =
                (std::max)(-lockOnCandidate_.screenPosition.y, 0.0f) +
                (std::max)(lockOnCandidate_.screenPosition.y - 1.0f, 0.0f);
            if (overflowX_ > 0.35f || overflowY_ > 0.35f) continue;
        }

        candidates_.push_back(lockOnCandidate_);
    }

    return candidates_;
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
    const LockOnCandidate target_ = FindBestLockOnCandidate(playerTarget_);
    ApplyLockOnCandidate(target_);
    SetLockOnEnabled(!target_.combatOwner.expired() && !target_.focusTarget.expired());
    return lockOnEnabled;
}

bool CameraLockOnComponent::SwitchLockOnTarget(
    const DirectX::XMFLOAT2& inputDirection_)
{
    const std::shared_ptr<GameObject> target_ =
        FindDirectionalLockOnTarget(inputDirection_);
    if (!target_) return false;
    SetLockOnFocusTarget(target_);
    return true;
}

bool CameraLockOnComponent::SwitchLockOnTargetNext()
{
    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget.lock());
    if (candidates_.empty()) return false;

    const std::shared_ptr<GameObject> currentFocusTarget_ =
        lockOnFocusTarget.lock();
    std::vector<LockOnCandidate> sortedCandidates_ = candidates_;
    std::sort(
        sortedCandidates_.begin(),
        sortedCandidates_.end(),
        [](const LockOnCandidate& left_, const LockOnCandidate& right_)
        {
            if (left_.screenPosition.x != right_.screenPosition.x)
            {
                return left_.screenPosition.x < right_.screenPosition.x;
            }
            return left_.screenPosition.y < right_.screenPosition.y;
        });

    std::size_t currentIndex_ = sortedCandidates_.size();
    for (std::size_t index_ = 0; index_ < sortedCandidates_.size(); ++index_)
    {
        if (sortedCandidates_[index_].focusTarget.lock() == currentFocusTarget_)
        {
            currentIndex_ = index_;
            break;
        }
    }

    const std::size_t nextIndex_ =
        currentIndex_ == sortedCandidates_.size()
        ? 0
        : (currentIndex_ + 1) % sortedCandidates_.size();
    return ApplyLockOnCandidate(sortedCandidates_[nextIndex_]);
}

bool CameraLockOnComponent::SwitchLockOnTargetPrevious()
{
    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget.lock());
    if (candidates_.empty()) return false;

    const std::shared_ptr<GameObject> currentFocusTarget_ =
        lockOnFocusTarget.lock();
    std::vector<LockOnCandidate> sortedCandidates_ = candidates_;
    std::sort(
        sortedCandidates_.begin(),
        sortedCandidates_.end(),
        [](const LockOnCandidate& left_, const LockOnCandidate& right_)
        {
            if (left_.screenPosition.x != right_.screenPosition.x)
            {
                return left_.screenPosition.x < right_.screenPosition.x;
            }
            return left_.screenPosition.y < right_.screenPosition.y;
        });

    std::size_t currentIndex_ = sortedCandidates_.size();
    for (std::size_t index_ = 0; index_ < sortedCandidates_.size(); ++index_)
    {
        if (sortedCandidates_[index_].focusTarget.lock() == currentFocusTarget_)
        {
            currentIndex_ = index_;
            break;
        }
    }

    const std::size_t previousIndex_ =
        currentIndex_ == sortedCandidates_.size() || currentIndex_ == 0
        ? sortedCandidates_.size() - 1
        : currentIndex_ - 1;
    return ApplyLockOnCandidate(sortedCandidates_[previousIndex_]);
}

std::shared_ptr<GameObject> CameraLockOnComponent::FindDirectionalLockOnTarget(
    const DirectX::XMFLOAT2& inputDirection_) const
{
    const float inputLength_ = std::sqrt(
        inputDirection_.x * inputDirection_.x +
        inputDirection_.y * inputDirection_.y);
    if (inputLength_ <= 0.0001f) return nullptr;

    const DirectX::XMFLOAT2 normalizedInput_
    {
        inputDirection_.x / inputLength_,
        inputDirection_.y / inputLength_
    };
    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget.lock());
    const std::shared_ptr<GameObject> currentFocusTarget_ =
        lockOnFocusTarget.lock();
    if (candidates_.empty() || !currentFocusTarget_) return nullptr;

    DirectX::XMFLOAT2 currentScreen_{ 0.5f, 0.5f };
    bool foundCurrentScreen_ = false;
    for (const LockOnCandidate& candidate_ : candidates_)
    {
        if (candidate_.focusTarget.lock() == currentFocusTarget_)
        {
            currentScreen_ = candidate_.screenPosition;
            foundCurrentScreen_ = true;
            break;
        }
    }
    if (!foundCurrentScreen_) return nullptr;

    LockOnCandidate bestCandidate_{};
    float bestScore_ = -(std::numeric_limits<float>::max)();
    for (LockOnCandidate candidate_ : candidates_)
    {
        if (candidate_.focusTarget.lock() == currentFocusTarget_) continue;

        const DirectX::XMFLOAT2 screenDifference_
        {
            candidate_.screenPosition.x - currentScreen_.x,
            candidate_.screenPosition.y - currentScreen_.y
        };
        const float screenDistance_ = std::sqrt(
            screenDifference_.x * screenDifference_.x +
            screenDifference_.y * screenDifference_.y);
        if (screenDistance_ <= 0.0001f) continue;

        const DirectX::XMFLOAT2 candidateDirection_
        {
            screenDifference_.x / screenDistance_,
            screenDifference_.y / screenDistance_
        };
        const float directionDot_ =
            normalizedInput_.x * candidateDirection_.x +
            normalizedInput_.y * candidateDirection_.y;
        if (directionDot_ < directionalDotThreshold) continue;

        const float centerX_ = candidate_.screenPosition.x - 0.5f;
        const float centerY_ = candidate_.screenPosition.y - 0.5f;
        const float centerDistance_ =
            std::sqrt(centerX_ * centerX_ + centerY_ * centerY_);
        candidate_.score =
            directionDot_ * 3.0f -
            screenDistance_ * screenDistanceWeight -
            centerDistance_ * centerDistanceWeight +
            candidate_.priority * priorityWeight +
            (candidate_.isDefaultTarget ? 0.2f : 0.0f);

        if (candidate_.score > bestScore_)
        {
            bestScore_ = candidate_.score;
            bestCandidate_ = candidate_;
        }
    }

    debugCandidateCount = static_cast<int>(candidates_.size());
    debugCurrentFocusScreen = currentScreen_;
    debugLastSwitchScore = bestScore_ == -(std::numeric_limits<float>::max)()
        ? 0.0f
        : bestScore_;
    return bestCandidate_.focusTarget.lock();
}

bool CameraLockOnComponent::ApplyLockOnCandidate(
    const LockOnCandidate& candidate_)
{
    const std::shared_ptr<GameObject> combatOwner_ =
        candidate_.combatOwner.lock();
    const std::shared_ptr<GameObject> focusTarget_ =
        candidate_.focusTarget.lock();
    if (!combatOwner_ || !focusTarget_) return false;

    lockOnTarget = combatOwner_;
    lockOnTargetName = combatOwner_->GetName();
    lockOnFocusTarget = focusTarget_;
    lockOnFocusTargetName = focusTarget_->GetName();
    debugLastSwitchScore = candidate_.score;
    return true;
}

bool CameraLockOnComponent::ResolveFocusTargetFallback()
{
    const std::shared_ptr<GameObject> combatOwner_ = lockOnTarget.lock();
    if (!combatOwner_) return false;

    const std::shared_ptr<GameObject> focusTarget_ = lockOnFocusTarget.lock();
    if (focusTarget_)
    {
        if (const std::shared_ptr<LockOnTargetComponent> lockTarget_ =
            focusTarget_->GetComponent<LockOnTargetComponent>())
        {
            const std::shared_ptr<GameObject> focusCombatOwner_ =
                lockTarget_->GetCombatOwner();
            if (focusCombatOwner_ == combatOwner_ && lockTarget_->IsLockOnEnabled())
            {
                return true;
            }
        }
        else if (focusTarget_ == combatOwner_)
        {
            return true;
        }
    }

    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget.lock());
    LockOnCandidate fallbackCandidate_{};
    bool hasFallback_ = false;
    float bestScore_ = (std::numeric_limits<float>::max)();
    for (LockOnCandidate candidate_ : candidates_)
    {
        if (candidate_.combatOwner.lock() != combatOwner_) continue;
        const float centerX_ = candidate_.screenPosition.x - 0.5f;
        const float centerY_ = candidate_.screenPosition.y - 0.5f;
        const float centerDistance_ = std::sqrt(centerX_ * centerX_ + centerY_ * centerY_);
        candidate_.score =
            centerDistance_ -
            candidate_.priority * 0.05f -
            (candidate_.isDefaultTarget ? 1.0f : 0.0f) +
            (candidate_.isCombatOwnerSelf ? 0.02f : 0.0f);
        if (candidate_.score < bestScore_)
        {
            bestScore_ = candidate_.score;
            fallbackCandidate_ = candidate_;
            hasFallback_ = true;
        }
    }
    if (hasFallback_) return ApplyLockOnCandidate(fallbackCandidate_);

    if (combatOwner_->GetComponent<LockOnTargetComponent>())
    {
        lockOnFocusTarget = combatOwner_;
        lockOnFocusTargetName = combatOwner_->GetName();
        return true;
    }
    if (combatOwner_->transform)
    {
        lockOnFocusTarget = combatOwner_;
        lockOnFocusTargetName = combatOwner_->GetName();
        return true;
    }
    return false;
}

void CameraLockOnComponent::UpdateLockOnSwitchInput(float deltaTime_)
{
    DirectX::XMFLOAT2 inputDirection_ = GetKeyboardSwitchInput();
    const DirectX::XMFLOAT2 controllerInput_ = GetControllerSwitchInput();
    debugControllerSwitchInput = controllerInput_;

    const float controllerLength_ = std::sqrt(
        controllerInput_.x * controllerInput_.x +
        controllerInput_.y * controllerInput_.y);
    if (controllerLength_ > 0.0001f)
    {
        inputDirection_ = controllerInput_;
    }

    const DirectX::XMFLOAT2 mouseInput_ = GetMouseSwitchInput(deltaTime_);
    const float mouseLength_ = std::sqrt(
        mouseInput_.x * mouseInput_.x +
        mouseInput_.y * mouseInput_.y);
    if (mouseLength_ > 0.0001f)
    {
        inputDirection_ = mouseInput_;
    }

    const float wheelAxis_ = KeyConfig::Instance().GetRawAxis("LockOn_SwitchWheel");
    const int wheelDirection_ = wheelAxis_ > 0.0f ? 1 : wheelAxis_ < 0.0f ? -1 : 0;
    if (wheelDirection_ != 0)
    {
        MouseInput::Instance().ConsumeWheelDelta();
    }
    if (switchCooldownTimer <= 0.0f && wheelDirection_ != 0)
    {
        const bool switched_ = wheelDirection_ > 0
            ? SwitchLockOnTargetNext()
            : SwitchLockOnTargetPrevious();
        if (switched_)
        {
            debugLastSwitchDirection = wheelDirection_ > 0
                ? DirectX::XMFLOAT2{ 1.0f, 0.0f }
                : DirectX::XMFLOAT2{ -1.0f, 0.0f };
            switchCooldownTimer = switchCooldown;
            accumulatedMouseDelta = { 0.0f, 0.0f };
            debugMouseAccumulatedDelta = accumulatedMouseDelta;
        }
        return;
    }

    const float inputLength_ = std::sqrt(
        inputDirection_.x * inputDirection_.x +
        inputDirection_.y * inputDirection_.y);
    if (inputLength_ <= switchStickReleaseThreshold)
    {
        switchInputLatched = false;
        return;
    }
    if (inputLength_ < switchStickThreshold || switchInputLatched ||
        switchCooldownTimer > 0.0f)
    {
        return;
    }

    const DirectX::XMFLOAT2 normalizedInput_
    {
        inputDirection_.x / inputLength_,
        inputDirection_.y / inputLength_
    };
    if (SwitchLockOnTarget(normalizedInput_))
    {
        debugLastSwitchDirection = normalizedInput_;
        switchInputLatched = true;
        switchCooldownTimer = switchCooldown;
        accumulatedMouseDelta = { 0.0f, 0.0f };
        debugMouseAccumulatedDelta = accumulatedMouseDelta;
    }
}

DirectX::XMFLOAT2 CameraLockOnComponent::GetKeyboardSwitchInput() const
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    DirectX::XMFLOAT2 input_{ 0.0f, 0.0f };
    if (keyConfig_.ActionPressedIn("LockOn_SwitchLeft")) input_.x -= 1.0f;
    if (keyConfig_.ActionPressedIn("LockOn_SwitchRight")) input_.x += 1.0f;
    if (keyConfig_.ActionPressedIn("LockOn_SwitchUp")) input_.y -= 1.0f;
    if (keyConfig_.ActionPressedIn("LockOn_SwitchDown")) input_.y += 1.0f;
    return input_;
}

DirectX::XMFLOAT2 CameraLockOnComponent::GetControllerSwitchInput() const
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    return
    {
        keyConfig_.GetAxis("LockOn_SwitchX"),
        -keyConfig_.GetAxis("LockOn_SwitchY")
    };
}

DirectX::XMFLOAT2 CameraLockOnComponent::GetMouseSwitchInput(float deltaTime_)
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    const DirectX::XMFLOAT2 mouseDelta_
    {
        keyConfig_.GetRawAxis("LockOn_SwitchMouseX"),
        keyConfig_.GetRawAxis("LockOn_SwitchMouseY")
    };
    if (std::abs(mouseDelta_.x) > 0.001f || std::abs(mouseDelta_.y) > 0.001f)
    {
        accumulatedMouseDelta.x += mouseDelta_.x;
        accumulatedMouseDelta.y += mouseDelta_.y;
        mouseAccumulationTimer = 0.0f;
    }
    else
    {
        mouseAccumulationTimer += (std::max)(deltaTime_, 0.0f);
        if (mouseAccumulationTimer >= mouseAccumulationResetTime)
        {
            accumulatedMouseDelta = { 0.0f, 0.0f };
        }
    }
    debugMouseAccumulatedDelta = accumulatedMouseDelta;

    const float absoluteX_ = std::abs(accumulatedMouseDelta.x);
    const float absoluteY_ = std::abs(accumulatedMouseDelta.y);
    if (absoluteX_ < mouseSwitchThreshold && absoluteY_ < mouseSwitchThreshold)
    {
        return { 0.0f, 0.0f };
    }

    if (absoluteX_ >= absoluteY_)
    {
        return { accumulatedMouseDelta.x >= 0.0f ? 1.0f : -1.0f, 0.0f };
    }
    return { 0.0f, accumulatedMouseDelta.y >= 0.0f ? 1.0f : -1.0f };
}

void CameraLockOnComponent::RegisterDefaultSwitchInputBindings()
{
    KeyConfig& keyConfig_ = KeyConfig::Instance();
    if (keyConfig_.GetActionMap().count("LockOn_SwitchLeft") == 0)
    {
        keyConfig_.BindAction(
            "LockOn_SwitchLeft",
            { { DeviceType::KBM, KeyInput::K_Left } });
    }
    if (keyConfig_.GetActionMap().count("LockOn_SwitchRight") == 0)
    {
        keyConfig_.BindAction(
            "LockOn_SwitchRight",
            { { DeviceType::KBM, KeyInput::K_Right } });
    }
    if (keyConfig_.GetActionMap().count("LockOn_SwitchUp") == 0)
    {
        keyConfig_.BindAction(
            "LockOn_SwitchUp",
            { { DeviceType::KBM, KeyInput::K_Up } });
    }
    if (keyConfig_.GetActionMap().count("LockOn_SwitchDown") == 0)
    {
        keyConfig_.BindAction(
            "LockOn_SwitchDown",
            { { DeviceType::KBM, KeyInput::K_Down } });
    }
    if (keyConfig_.GetAxisMap().count("LockOn_SwitchX") == 0)
    {
        keyConfig_.BindAxis(
            "LockOn_SwitchX",
            AxisUnit{
                AxisSourceType::GamepadStick,
                DeviceType::Gamepad,
                static_cast<int>(AxisCode::RightStickX),
                1.0f,
                switchStickReleaseThreshold });
    }
    if (keyConfig_.GetAxisMap().count("LockOn_SwitchY") == 0)
    {
        keyConfig_.BindAxis(
            "LockOn_SwitchY",
            AxisUnit{
                AxisSourceType::GamepadStick,
                DeviceType::Gamepad,
                static_cast<int>(AxisCode::RightStickY),
                1.0f,
                switchStickReleaseThreshold });
    }
    if (keyConfig_.GetAxisMap().count("LockOn_SwitchMouseX") == 0)
    {
        keyConfig_.BindAxis(
            "LockOn_SwitchMouseX",
            AxisUnit{
                AxisSourceType::MouseMove,
                DeviceType::KBM,
                static_cast<int>(AxisCode::MouseX),
                1.0f,
                0.0f });
    }
    if (keyConfig_.GetAxisMap().count("LockOn_SwitchMouseY") == 0)
    {
        keyConfig_.BindAxis(
            "LockOn_SwitchMouseY",
            AxisUnit{
                AxisSourceType::MouseMove,
                DeviceType::KBM,
                static_cast<int>(AxisCode::MouseY),
                1.0f,
                0.0f });
    }
    if (keyConfig_.GetAxisMap().count("LockOn_SwitchWheel") == 0)
    {
        keyConfig_.BindAxis(
            "LockOn_SwitchWheel",
            AxisUnit{
                AxisSourceType::MouseMove,
                DeviceType::KBM,
                static_cast<int>(AxisCode::MouseWheel),
                1.0f / 120.0f,
                0.0f });
    }
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
    const std::shared_ptr<GameObject> target_ = lockOnFocusTarget.lock();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!player_ || !target_ || !camera_ ||
        !player_->transform || !target_->transform)
    {
        return false;
    }
    SynchronizeExternalFovChange(camera_);

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
    if (lockOnFocusTarget.expired() && !lockOnFocusTargetName.empty())
    {
        lockOnFocusTarget = owner_->manager->Find(lockOnFocusTargetName);
    }
}

void CameraLockOnComponent::RegisterActions()
{
    RegisterDefaultSwitchInputBindings();
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
    registry_.RegisterAction("Camera", "LockOn_SwitchLeft", [this]()
    {
        SwitchLockOnTarget({ -1.0f, 0.0f });
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchRight", [this]()
    {
        SwitchLockOnTarget({ 1.0f, 0.0f });
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchUp", [this]()
    {
        SwitchLockOnTarget({ 0.0f, -1.0f });
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchDown", [this]()
    {
        SwitchLockOnTarget({ 0.0f, 1.0f });
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchNext", [this]()
    {
        SwitchLockOnTargetNext();
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchPrevious", [this]()
    {
        SwitchLockOnTargetPrevious();
    });
}

void CameraLockOnComponent::ResetTracking()
{
    hasTrackingPose = false;
    hasCollisionComposition = false;
    currentWallCameraRate = 0.0f;
    currentWallCameraSide = 0;
    wallCameraSideHoldTimer = 0.0f;
    hasWallCameraPosition = false;
    hasArenaOutwardDirection = false;
    previousLockOnTarget.reset();
    previousFocusTarget.reset();
    switchCooldownTimer = 0.0f;
    switchInputLatched = false;
    mouseAccumulationTimer = 0.0f;
    accumulatedMouseDelta = { 0.0f, 0.0f };
    debugMouseAccumulatedDelta = accumulatedMouseDelta;
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

void CameraLockOnComponent::SynchronizeExternalFovChange(
    const std::shared_ptr<CameraComponent>& camera_)
{
    BS_PROFILE_SCOPE("CameraLockOnComponent::SynchronizeExternalFovChange");

    if (!camera_) return;

    const float cameraFov_ = camera_->GetFovY();
    if (!hasBaseFov)
    {
        baseFovY = cameraFov_;
        currentCollisionFov = cameraFov_;
        hasBaseFov = true;
        return;
    }

    constexpr float externalFovThreshold_ = 0.05f;
    if (std::abs(cameraFov_ - currentCollisionFov) <= externalFovThreshold_)
    {
        return;
    }

    const float lockOnFovAddition_ = currentCollisionFov - baseFovY;
    baseFovY = std::clamp(cameraFov_ - lockOnFovAddition_, 1.0f, 170.0f);
    currentCollisionFov = cameraFov_;
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

DirectX::XMFLOAT3 CameraLockOnComponent::ClampCameraPositionToArena(
    const DirectX::XMFLOAT3& cameraPosition_) const
{
    DirectX::XMFLOAT3 clampedPosition_ = cameraPosition_;
    const float maximumCameraRadius_ = (std::max)(
        arenaRadius - (std::max)(arenaCameraMargin, 0.0f),
        0.001f);
    const DirectX::XMFLOAT3 centerToCamera_ =
        SubtractFloat3(cameraPosition_, arenaCenter);
    const float cameraArenaRadius_ = LengthHorizontal(centerToCamera_);

    if (!enableCircularArenaCamera ||
        arenaRadius <= 0.001f ||
        arenaRadius <= arenaCameraMargin + 0.001f ||
        cameraArenaRadius_ <= maximumCameraRadius_)
    {
        return clampedPosition_;
    }

    // 円形外周はPhysX結果だけに任せず、中心と半径から最終安全位置を決める。
    const DirectX::XMFLOAT3 direction_ = NormalizeHorizontal(
        centerToCamera_,
        hasArenaOutwardDirection
            ? previousArenaOutwardDirection
            : SubtractFloat3(interpolatedPosition, arenaCenter));
    clampedPosition_.x = arenaCenter.x + direction_.x * maximumCameraRadius_;
    clampedPosition_.z = arenaCenter.z + direction_.z * maximumCameraRadius_;
    return clampedPosition_;
}

float CameraLockOnComponent::CalculateWallCameraCandidateScore(
    const DirectX::XMFLOAT3& candidatePosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float fovY_,
    int candidateSide_) const
{
    DirectX::XMFLOAT2 playerTopScreen_{};
    DirectX::XMFLOAT2 playerBottomScreen_{};
    DirectX::XMFLOAT2 targetScreen_{};
    const bool playerTopVisible_ = ProjectToScreenPosition(
        candidatePosition_,
        lookAtPosition_,
        playerTopPosition_,
        fovY_,
        playerTopScreen_);
    const bool playerBottomVisible_ = ProjectToScreenPosition(
        candidatePosition_,
        lookAtPosition_,
        playerBottomPosition_,
        fovY_,
        playerBottomScreen_);
    const bool targetVisible_ = ProjectToScreenPosition(
        candidatePosition_,
        lookAtPosition_,
        targetPosition_,
        fovY_,
        targetScreen_);
    const DirectX::XMFLOAT3 clampedPosition_ =
        ClampCameraPositionToArena(candidatePosition_);
    const float clampDistance_ = LengthHorizontal(
        SubtractFloat3(candidatePosition_, clampedPosition_));
    const float sideHoldBonus_ = candidateSide_ == currentWallCameraSide
        ? -wallCameraSideSwitchScoreThreshold
        : 0.0f;
    return CalculateProjectionScore(
        playerTopScreen_,
        playerBottomScreen_,
        targetScreen_,
        playerTopVisible_,
        playerBottomVisible_,
        targetVisible_) +
        clampDistance_ * 4.0f +
        (candidateSide_ == 0 ? 0.03f : 0.0f) +
        sideHoldBonus_;
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
    const float wallFovAddition_ = enableCircularArenaCamera
        ? wallCameraMaximumFovAddition * SmoothStep(currentWallCameraRate)
        : 0.0f;
    const float baseFovWithWall_ = std::clamp(
        baseFovY + wallFovAddition_,
        20.0f,
        120.0f);
    float bestFov_ = baseFovWithWall_;
    float bestScore_ = (std::numeric_limits<float>::max)();
    const float maxFov_ = std::clamp(
        baseFovWithWall_ + collisionFovMaxAddition * compressionRate_,
        baseFovWithWall_,
        120.0f);
    constexpr int candidateCount_ = 5;
    for (int index_ = 0; index_ < candidateCount_; ++index_)
    {
        const float rate_ =
            static_cast<float>(index_) /
            static_cast<float>(candidateCount_ - 1);
        const float fov_ = std::lerp(baseFovWithWall_, maxFov_, rate_);
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
    const std::shared_ptr<GameObject> focusTarget_ = lockOnFocusTarget.lock();

    if (!player_ || !target_ || !focusTarget_ ||
        !player_->transform || !target_->transform || !focusTarget_->transform)
    {
        return info_;
    }

    info_.playerPosition = GetFocusPosition(player_, playerOffset);
    info_.targetPosition = GetFocusPosition(focusTarget_, targetOffset);
    const DirectX::XMFLOAT3 combatTargetPosition_ =
        GetFocusPosition(target_, targetOffset);

    info_.difference =
    {
        info_.playerPosition.x - combatTargetPosition_.x,
        info_.playerPosition.y - combatTargetPosition_.y,
        info_.playerPosition.z - combatTargetPosition_.z
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
    ImGui::Text("Composition Weight: %.3f", compositionCorrection.weight);
    ImGui::Text(
        "Composition Offset Side/Distance/Height: %.3f / %.3f / %.3f",
        compositionCorrection.sideOffset,
        compositionCorrection.distanceOffset,
        compositionCorrection.heightOffset);

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
    if (ImGui::InputText("Combat Owner", targetBuffer_, sizeof(targetBuffer_)))
    {
        lockOnTargetName = targetBuffer_;
        lockOnTarget.reset();
        ResolveReferences();
        ResetTracking();
    }
    char focusTargetBuffer_[128]{};
    strcpy_s(focusTargetBuffer_, lockOnFocusTargetName.c_str());
    if (ImGui::InputText("Focus Target", focusTargetBuffer_, sizeof(focusTargetBuffer_)))
    {
        lockOnFocusTargetName = focusTargetBuffer_;
        lockOnFocusTarget.reset();
        ResolveReferences();
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
    ImGui::SeparatorText("Circular Arena Wall Camera");
    ImGui::Checkbox("Enable Circular Arena Camera", &enableCircularArenaCamera);
    ImGui::DragFloat3("Arena Center", &arenaCenter.x, 0.05f);
    ImGui::DragFloat("Arena Radius", &arenaRadius, 0.1f, 0.1f, 500.0f);
    ImGui::DragFloat("Arena Camera Margin", &arenaCameraMargin, 0.05f, 0.0f, 50.0f);
    ImGui::DragFloat("Wall Camera Start Distance", &wallCameraStartDistance, 0.05f, 0.1f, 50.0f);
    ImGui::DragFloat("Wall Camera Distance", &wallCameraDistance, 0.05f, 0.1f, 50.0f);
    ImGui::DragFloat("Wall Camera Height", &wallCameraHeight, 0.05f, -20.0f, 50.0f);
    ImGui::DragFloat("Wall Camera Side Offset", &wallCameraSideOffset, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("Wall Center Weight", &wallCenterWeight, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Wall Combat Weight", &wallCombatWeight, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Wall Position Speed", &wallCameraPositionSmoothSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Wall Rate Speed", &wallCameraRateSmoothSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Wall Height Addition", &wallCameraMaximumHeightAddition, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("Wall FOV Addition", &wallCameraMaximumFovAddition, 0.5f, 0.0f, 60.0f);
    ImGui::DragFloat("Wall LookAt Player Bias", &wallLookAtPlayerBias, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Wall Side Hold Time", &wallCameraSideMinimumHoldTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Wall Switch Score Threshold", &wallCameraSideSwitchScoreThreshold, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(
        "Wall Opposite Side Hold Time",
        &wallCameraOppositeSideMinimumHoldTime,
        0.01f,
        0.0f,
        5.0f);
    ImGui::DragFloat(
        "Wall Opposite Switch Score Threshold",
        &wallCameraOppositeSideSwitchScoreThreshold,
        0.01f,
        0.0f,
        10.0f);
    ImGui::DragFloat(
        "Wall Continuity Score Weight",
        &wallCameraContinuityScoreWeight,
        0.01f,
        0.0f,
        2.0f);
    ImGui::SeparatorText("LockOn Target Switch");
    ImGui::DragFloat("Stick Switch Threshold", &switchStickThreshold, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Stick Release Threshold", &switchStickReleaseThreshold, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Mouse Switch Threshold", &mouseSwitchThreshold, 1.0f, 1.0f, 500.0f);
    ImGui::DragFloat("Mouse Accumulation Reset Time", &mouseAccumulationResetTime, 0.01f, 0.01f, 1.0f);
    ImGui::DragFloat("Switch Cooldown", &switchCooldown, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Directional Dot Threshold", &directionalDotThreshold, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat("Screen Distance Weight", &screenDistanceWeight, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Center Distance Weight", &centerDistanceWeight, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Priority Weight", &priorityWeight, 0.01f, 0.0f, 10.0f);
    ImGui::Text("Current Combat Owner: %s", lockOnTargetName.c_str());
    ImGui::Text("Current Focus Target: %s", lockOnFocusTargetName.c_str());
    ImGui::Text("Candidate Count: %d", debugCandidateCount);
    ImGui::Text(
        "Last Switch Direction: %.3f, %.3f",
        debugLastSwitchDirection.x,
        debugLastSwitchDirection.y);
    ImGui::Text(
        "Controller Input: %.3f, %.3f",
        debugControllerSwitchInput.x,
        debugControllerSwitchInput.y);
    ImGui::Text(
        "Mouse Accumulated Delta: %.3f, %.3f",
        debugMouseAccumulatedDelta.x,
        debugMouseAccumulatedDelta.y);
    ImGui::Text("Switch Latched: %s", switchInputLatched ? "true" : "false");
    ImGui::Text("Switch Cooldown Timer: %.3f", switchCooldownTimer);
    ImGui::Text("Last Switch Score: %.3f", debugLastSwitchScore);
    ImGui::Text(
        "Current Focus Screen: %.3f, %.3f",
        debugCurrentFocusScreen.x,
        debugCurrentFocusScreen.y);
    ImGui::Text("Distance To Wall: %.3f", debugDistanceToWall);
    ImGui::Text("Raw Wall Camera Rate: %.3f", debugRawWallCameraRate);
    ImGui::Text("Current Wall Camera Rate: %.3f", currentWallCameraRate);
    ImGui::Text("Current Wall Camera Side: %d", currentWallCameraSide);
    ImGui::Text("Wall Camera Side Hold Timer: %.3f", wallCameraSideHoldTimer);
    ImGui::Text("Selected Candidate Score: %.3f", debugSelectedCandidateScore);
    ImGui::Text(
        "Candidate Scores L/C/R: %.3f / %.3f / %.3f",
        debugLeftCandidateScore,
        debugCenterCandidateScore,
        debugRightCandidateScore);
    ImGui::Text(
        "Normal Camera Position: %.3f, %.3f, %.3f",
        debugNormalCameraPosition.x,
        debugNormalCameraPosition.y,
        debugNormalCameraPosition.z);
    ImGui::Text(
        "Selected Wall Camera Position: %.3f, %.3f, %.3f",
        debugSelectedWallCameraPosition.x,
        debugSelectedWallCameraPosition.y,
        debugSelectedWallCameraPosition.z);
    ImGui::Text(
        "Final Desired Camera Position: %.3f, %.3f, %.3f",
        debugFinalDesiredCameraPosition.x,
        debugFinalDesiredCameraPosition.y,
        debugFinalDesiredCameraPosition.z);
    ImGui::Text("Is Wall Camera Active: %s", IsWallCameraActive() ? "true" : "false");
    ImGui::Text(
        "Camera Arena Radius: %.3f / Maximum: %.3f",
        debugCameraArenaRadius,
        debugMaximumCameraArenaRadius);
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
    ImGui::TextDisabled(
        "Actions: LockOn_Enable / Disable / Toggle / SwitchSide / SwitchLeft / SwitchRight / SwitchUp / SwitchDown / SwitchNext / SwitchPrevious");
}

void CameraLockOnComponent::Serialize(nlohmann::json& json_) const
{
    json_["playerTargetName"] = playerTargetName;
    json_["lockOnTargetName"] = lockOnTargetName;
    json_["lockOnFocusTargetName"] = lockOnFocusTargetName;
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
    json_["enableCircularArenaCamera"] = enableCircularArenaCamera;
    json_["arenaCenter"] = { arenaCenter.x, arenaCenter.y, arenaCenter.z };
    json_["arenaRadius"] = arenaRadius;
    json_["arenaCameraMargin"] = arenaCameraMargin;
    json_["wallCameraStartDistance"] = wallCameraStartDistance;
    json_["wallCameraDistance"] = wallCameraDistance;
    json_["wallCameraHeight"] = wallCameraHeight;
    json_["wallCameraSideOffset"] = wallCameraSideOffset;
    json_["wallCenterWeight"] = wallCenterWeight;
    json_["wallCombatWeight"] = wallCombatWeight;
    json_["wallCameraPositionSmoothSpeed"] = wallCameraPositionSmoothSpeed;
    json_["wallCameraRateSmoothSpeed"] = wallCameraRateSmoothSpeed;
    json_["wallCameraMaximumHeightAddition"] =
        wallCameraMaximumHeightAddition;
    json_["wallCameraMaximumFovAddition"] =
        wallCameraMaximumFovAddition;
    json_["wallLookAtPlayerBias"] = wallLookAtPlayerBias;
    json_["wallCameraSideMinimumHoldTime"] =
        wallCameraSideMinimumHoldTime;
    json_["wallCameraSideSwitchScoreThreshold"] =
        wallCameraSideSwitchScoreThreshold;
    json_["wallCameraOppositeSideMinimumHoldTime"] =
        wallCameraOppositeSideMinimumHoldTime;
    json_["wallCameraOppositeSideSwitchScoreThreshold"] =
        wallCameraOppositeSideSwitchScoreThreshold;
    json_["wallCameraContinuityScoreWeight"] =
        wallCameraContinuityScoreWeight;
    json_["switchStickThreshold"] = switchStickThreshold;
    json_["switchStickReleaseThreshold"] = switchStickReleaseThreshold;
    json_["switchCooldown"] = switchCooldown;
    json_["mouseSwitchThreshold"] = mouseSwitchThreshold;
    json_["mouseAccumulationResetTime"] = mouseAccumulationResetTime;
    json_["directionalDotThreshold"] = directionalDotThreshold;
    json_["screenDistanceWeight"] = screenDistanceWeight;
    json_["centerDistanceWeight"] = centerDistanceWeight;
    json_["priorityWeight"] = priorityWeight;
}

void CameraLockOnComponent::Deserialize(const nlohmann::json& json_)
{
    playerTargetName = json_.value("playerTargetName", std::string{});
    lockOnTargetName = json_.value("lockOnTargetName", std::string{});
    lockOnFocusTargetName = json_.value("lockOnFocusTargetName", std::string{});
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
    enableCircularArenaCamera =
        json_.value("enableCircularArenaCamera", false);
    loadFloat3_("arenaCenter", arenaCenter);
    arenaRadius = json_.value("arenaRadius", 15.0f);
    arenaCameraMargin = json_.value("arenaCameraMargin", 0.8f);
    wallCameraStartDistance =
        json_.value("wallCameraStartDistance", 3.0f);
    wallCameraDistance = json_.value("wallCameraDistance", 5.5f);
    wallCameraHeight = json_.value("wallCameraHeight", 3.0f);
    wallCameraSideOffset = json_.value("wallCameraSideOffset", 1.5f);
    wallCenterWeight = json_.value("wallCenterWeight", 0.75f);
    wallCombatWeight = json_.value("wallCombatWeight", 0.25f);
    wallCameraPositionSmoothSpeed =
        json_.value("wallCameraPositionSmoothSpeed", 8.0f);
    wallCameraRateSmoothSpeed =
        json_.value("wallCameraRateSmoothSpeed", 7.0f);
    wallCameraMaximumHeightAddition =
        json_.value("wallCameraMaximumHeightAddition", 1.2f);
    wallCameraMaximumFovAddition =
        json_.value("wallCameraMaximumFovAddition", 12.0f);
    wallLookAtPlayerBias =
        json_.value("wallLookAtPlayerBias", 0.4f);
    wallCameraSideMinimumHoldTime =
        json_.value("wallCameraSideMinimumHoldTime", 0.5f);
    wallCameraSideSwitchScoreThreshold =
        json_.value("wallCameraSideSwitchScoreThreshold", 0.35f);
    wallCameraOppositeSideMinimumHoldTime =
        json_.value("wallCameraOppositeSideMinimumHoldTime", 0.85f);
    wallCameraOppositeSideSwitchScoreThreshold =
        json_.value("wallCameraOppositeSideSwitchScoreThreshold", 0.75f);
    wallCameraContinuityScoreWeight =
        json_.value("wallCameraContinuityScoreWeight", 0.12f);
    switchStickThreshold =
        json_.value("switchStickThreshold", 0.65f);
    switchStickReleaseThreshold =
        json_.value("switchStickReleaseThreshold", 0.25f);
    switchCooldown =
        json_.value("switchCooldown", 0.18f);
    mouseSwitchThreshold =
        json_.value("mouseSwitchThreshold", 35.0f);
    mouseAccumulationResetTime =
        json_.value("mouseAccumulationResetTime", 0.12f);
    directionalDotThreshold =
        json_.value("directionalDotThreshold", 0.35f);
    screenDistanceWeight =
        json_.value("screenDistanceWeight", 0.55f);
    centerDistanceWeight =
        json_.value("centerDistanceWeight", 0.35f);
    priorityWeight =
        json_.value("priorityWeight", 0.25f);

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
    arenaRadius = (std::max)(arenaRadius, 0.001f);
    arenaCameraMargin = std::clamp(arenaCameraMargin, 0.0f, arenaRadius - 0.001f);
    wallCameraStartDistance = std::clamp(wallCameraStartDistance, 0.1f, 50.0f);
    wallCameraDistance = std::clamp(wallCameraDistance, 0.1f, 50.0f);
    wallCameraHeight = std::clamp(wallCameraHeight, -20.0f, 50.0f);
    wallCameraSideOffset = std::clamp(wallCameraSideOffset, 0.0f, 20.0f);
    wallCenterWeight = std::clamp(wallCenterWeight, 0.0f, 10.0f);
    wallCombatWeight = std::clamp(wallCombatWeight, 0.0f, 10.0f);
    wallCameraPositionSmoothSpeed =
        std::clamp(wallCameraPositionSmoothSpeed, 0.1f, 100.0f);
    wallCameraRateSmoothSpeed =
        std::clamp(wallCameraRateSmoothSpeed, 0.1f, 100.0f);
    wallCameraMaximumHeightAddition =
        std::clamp(wallCameraMaximumHeightAddition, 0.0f, 10.0f);
    wallCameraMaximumFovAddition =
        std::clamp(wallCameraMaximumFovAddition, 0.0f, 60.0f);
    wallLookAtPlayerBias = std::clamp(wallLookAtPlayerBias, 0.0f, 1.0f);
    wallCameraSideMinimumHoldTime =
        std::clamp(wallCameraSideMinimumHoldTime, 0.0f, 5.0f);
    wallCameraSideSwitchScoreThreshold =
        std::clamp(wallCameraSideSwitchScoreThreshold, 0.0f, 10.0f);
    wallCameraOppositeSideMinimumHoldTime =
        std::clamp(wallCameraOppositeSideMinimumHoldTime, 0.0f, 5.0f);
    wallCameraOppositeSideSwitchScoreThreshold =
        std::clamp(wallCameraOppositeSideSwitchScoreThreshold, 0.0f, 10.0f);
    wallCameraContinuityScoreWeight =
        std::clamp(wallCameraContinuityScoreWeight, 0.0f, 2.0f);
    switchStickReleaseThreshold =
        std::clamp(switchStickReleaseThreshold, 0.0f, 1.0f);
    switchStickThreshold =
        std::clamp(switchStickThreshold, switchStickReleaseThreshold, 1.0f);
    switchCooldown = std::clamp(switchCooldown, 0.0f, 2.0f);
    mouseSwitchThreshold = std::clamp(mouseSwitchThreshold, 1.0f, 500.0f);
    mouseAccumulationResetTime =
        std::clamp(mouseAccumulationResetTime, 0.01f, 1.0f);
    directionalDotThreshold =
        std::clamp(directionalDotThreshold, -1.0f, 1.0f);
    screenDistanceWeight = std::clamp(screenDistanceWeight, 0.0f, 10.0f);
    centerDistanceWeight = std::clamp(centerDistanceWeight, 0.0f, 10.0f);
    priorityWeight = std::clamp(priorityWeight, 0.0f, 10.0f);
    ResetTracking();
}

REGISTER_COMPONENT(CameraLockOnComponent, "CameraLockOnComponent")
