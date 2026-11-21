#include "PrecompiledHeader.h"

#include "SpecialAttackCameraComponent.h"

#include "CameraActionDirectorComponent.h"
#include "CameraComponent.h"
#include "CameraLockOnComponent.h"
#include "LockOnTargetComponent.h"
#include "ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    // カメラ補正では水平面の向きと3D位置を何度も扱うため、このファイル内だけの軽量ヘルパーにまとめる。
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

SpecialAttackCameraComponent::SpecialAttackCameraComponent()
{
    SetLateUpdatePriority(1);
}

bool SpecialAttackCameraComponent::BeginTracking(
    const std::shared_ptr<GameObject>& player_,
    const std::shared_ptr<GameObject>& attacker_)
{
    const auto owner_ = GetGameObject();
    const auto camera_ = owner_ ? owner_->GetComponent<CameraComponent>() : nullptr;
    if (!IsEnabled() || !ShouldLateUpdate() || !camera_ || !player_ || !attacker_ ||
        !player_->transform || !attacker_->transform || !attacker_->IsCombatReady())
    {
        return false;
    }

    camera = camera_;
    playerTarget = player_;
    attackTarget = attacker_;
    baseFovY = camera_->GetFovY();
    currentCollisionFov = baseFovY;
    hasBaseFov = true;
    ResetTracking();
    return true;
}

bool SpecialAttackCameraComponent::IsTracking() const
{
    const auto player_ = playerTarget.lock();
    const auto attacker_ = attackTarget.lock();
    return IsEnabled() && ShouldLateUpdate() && player_ && player_->transform &&
        !player_->IsPendingDestroy() && attacker_ && attacker_->transform && attacker_->IsCombatReady();
}

void SpecialAttackCameraComponent::EndTracking()
{
    if (!hasBaseFov)
    {
        return;
    }
    hasBaseFov = false;
    playerTarget.reset();
    attackTarget.reset();
    ResetTracking();
    const auto owner_ = GetGameObject();
    const auto camera_ = camera.lock();
    if (!owner_ || !camera_)
    {
        return;
    }
    const auto director_ = owner_->GetComponent<CameraActionDirectorComponent>();
    if (director_ && director_->IsActionActive())
    {
        return;
    }
    camera_->SetFov(baseFovY);
    if (const auto thirdPerson_ = owner_->GetComponent<ThirdPersonCameraComponent>())
    {
        thirdPerson_->SynchronizeViewAngles(camera_->GetEye(), camera_->GetFocus());
        thirdPerson_->BeginReturnBlendFromCurrentCamera();
    }
}

void SpecialAttackCameraComponent::LateUpdate(float deltaTime_)
{
    if (!IsTracking())
    {
        if (hasBaseFov)
        {
            EndTracking();
        }
        return;
    }

    BS_PROFILE_SCOPE("SpecialAttackCameraComponent::LateUpdate");
    const auto owner_ = GetGameObject();
    const auto camera_ = camera.lock();
    if (!owner_ || !camera_)
    {
        EndTracking();
        return;
    }
    const auto director_ = owner_->GetComponent<CameraActionDirectorComponent>();
    if (director_ && director_->IsSuppressingGameplayCamera())
    {
        hasTrackingPose = false;
        return;
    }
    if (!director_ || !director_->IsActionActive())
    {
        SynchronizeExternalFovChange(camera_);
    }

    const auto player_ = playerTarget.lock();
    const auto thirdPerson_ = owner_->GetComponent<ThirdPersonCameraComponent>();
    const auto playerPosition_ = GetFocusPosition(player_, playerOffset);
    const auto enemyPosition_ = GetFocusPosition(attackTarget.lock(), targetOffset);
    const auto difference_ = SubtractFloat3(playerPosition_, enemyPosition_);
    const float horizontalDistance_ = LengthHorizontal(difference_);
    currentTargetDistance = std::hypot(horizontalDistance_, difference_.y);

    // プレイヤーから敵を見た反対方向を基準に、肩越しの横オフセットを加えた通常カメラ位置を作る。
    DirectX::XMFLOAT3 backDirection_{};
    if (horizontalDistance_ > 0.0001f)
    {
        backDirection_ =
        {
            difference_.x / horizontalDistance_,
            0.0f,
            difference_.z / horizontalDistance_
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

    if (enableCloseRangeDistanceGuard)
    {
        // 敵がPlayerを横切っても、近距離では一度のFrameで背面方向を反転させない。
        if (hasCloseRangeBackDirection && horizontalDistance_ < closeDistance)
        {
            if (horizontalDistance_ <= closeRangeDirectionHoldDistance)
            {
                backDirection_ = closeRangeBackDirection;
            }
            else
            {
                const float previousYaw_ = std::atan2(closeRangeBackDirection.x, closeRangeBackDirection.z);
                const float desiredYaw_ = std::atan2(backDirection_.x, backDirection_.z);
                const float differenceYaw_ = std::remainder(desiredYaw_ - previousYaw_, DirectX::XM_2PI);
                const float limit_ = (std::max)(0.0f, closeRangeOrbitTurnSpeed) * DirectX::XM_PI / 180.0f * (std::max)(deltaTime_, 0.0f);
                const float yaw_ = previousYaw_ + std::clamp(differenceYaw_, -limit_, limit_);
                backDirection_ = { std::sin(yaw_), 0.0f, std::cos(yaw_) };
            }
        }
        closeRangeBackDirection = backDirection_;
        hasCloseRangeBackDirection = true;
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
    // 近距離ではプレイヤー寄り、遠距離では敵寄りに注視点をずらして画面内の見やすさを保つ。
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

    const auto lockOn_ = owner_->GetComponent<CameraLockOnComponent>();
    const LockOnCameraCompositionCorrection compositionCorrection_ =
        lockOn_ && lockOn_->IsLockOnEnabled() ? lockOn_->GetCompositionCorrection() : LockOnCameraCompositionCorrection{};
    const float compositionWeight_ =
        std::clamp(compositionCorrection_.weight, 0.0f, 1.0f);
    if (compositionWeight_ > 0.0001f)
    {
        // 攻撃演出などから一時的な構図補正を受け取り、距離/高さ/注視点/FOVへなめらかに混ぜる。
        desiredPosition_ = AddFloat3(
            desiredPosition_,
            ScaleFloat3(
                backDirection_,
                compositionCorrection_.distanceOffset * compositionWeight_));
        desiredPosition_.y +=
            compositionCorrection_.heightOffset * compositionWeight_;
        desiredLookAt_.y +=
            compositionCorrection_.lookAtHeightOffset * compositionWeight_;
        desiredLookAt_ = AddFloat3(
            desiredLookAt_,
            ScaleFloat3(
                SubtractFloat3(enemyPosition_, playerPosition_),
                compositionCorrection_.focusTargetRateOffset * compositionWeight_));
    }
    if (enableCloseRangeDistanceGuard)
    {
        // 攻撃演出の構図補正後にも最短距離と高さを守り、地面との衝突で視点が潰れるのを防ぐ。
        const float x_ = desiredPosition_.x - playerPosition_.x;
        const float z_ = desiredPosition_.z - playerPosition_.z;
        const float horizontalLength_ = std::sqrt(x_ * x_ + z_ * z_);
        const float minimum_ = (std::max)(0.1f, minCameraDistance);
        if (horizontalLength_ < minimum_)
        {
            const float scale_ = horizontalLength_ > 0.0001f ? minimum_ / horizontalLength_ : 0.0f;
            desiredPosition_.x = playerPosition_.x + (scale_ > 0.0f ? x_ * scale_ : backDirection_.x * minimum_);
            desiredPosition_.z = playerPosition_.z + (scale_ > 0.0f ? z_ * scale_ : backDirection_.z * minimum_);
        }
        desiredPosition_.y = (std::max)(desiredPosition_.y, playerPosition_.y + (std::max)(0.0f, closeRangeMinimumHeight));
    }
    DirectX::XMFLOAT3 finalDesiredPosition_ = desiredPosition_;
    DirectX::XMFLOAT3 finalDesiredLookAt_ = desiredLookAt_;
    float wallFovAddition_ =
        compositionCorrection_.fovOffset * compositionWeight_;

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
            if (side_ < 0)
            {
                return debugLeftCandidateScore;
            }
            if (side_ > 0)
            {
                return debugRightCandidateScore;
            }
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
        // 壁カメラが強く効いているほど、演出側の左右寄せを弱めてアリーナ外へ出にくくする。
        const float sideAllowRate_ = std::clamp(
            1.0f - currentWallCameraRate * 0.8f,
            0.0f,
            1.0f);
        finalDesiredPosition_ = AddFloat3(
            finalDesiredPosition_,
            ScaleFloat3(
                sideDirection_,
                compositionCorrection_.sideOffset *
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

    if (!hasTrackingPose)
    {
        interpolatedPosition = camera_->GetEye();
        interpolatedLookAt = camera_->GetFocus();
        hasTrackingPose = true;
    }

    // 最終候補へ指数補間で追従し、deltaTimeが変わっても見た目の追従速度が極端に変わらないようにする。
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

bool SpecialAttackCameraComponent::ApplyCollisionComposition(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& baseLookAtPosition_,
    float compressionRate_,
    float collisionLift_,
    float deltaTime_)
{
    // 外部の衝突判定から押し戻し量を受け取り、注視点、距離、FOVを再評価して画面内の安全枠へ収める。
    if (!HasTrackingPose())
    {
        return false;
    }

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> target_ = attackTarget.lock();
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

void SpecialAttackCameraComponent::ResetTracking()
{
    hasCloseRangeBackDirection = false;
    hasTrackingPose = false;
    hasCollisionComposition = false;
    currentWallCameraRate = 0.0f;
    currentWallCameraSide = 0;
    wallCameraSideHoldTimer = 0.0f;
    hasWallCameraPosition = false;
    hasArenaOutwardDirection = false;
}

void SpecialAttackCameraComponent::ApplyCameraPose(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_)
{
    // LookAt行列からGameObjectのワールド姿勢を逆算し、CameraComponentにも同じ視点情報を反映する。
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

void SpecialAttackCameraComponent::SynchronizeExternalFovChange(
    const std::shared_ptr<CameraComponent>& camera_)
{
    // 他システムがFOVを変更した場合は、それを新しい基準として扱いロックオン補正量だけを保つ。
    if (!camera_)
    {
        return;
    }

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

DirectX::XMFLOAT3 SpecialAttackCameraComponent::GetFocusPosition(
    const std::shared_ptr<GameObject>& object_,
    const DirectX::XMFLOAT3& fallbackOffset_) const
{
    if (!object_ || !object_->transform)
    {
        return fallbackOffset_;
    }
    if (const std::shared_ptr<LockOnTargetComponent> lockTarget_ =
        object_->GetComponent<LockOnTargetComponent>())
    {
        return lockTarget_->GetFocusPosition();
    }
    return AddFloat3(object_->transform->GetWorldPosition(), fallbackOffset_);
}

DirectX::XMFLOAT3 SpecialAttackCameraComponent::GetPlayerBottomPosition(
    const std::shared_ptr<GameObject>& object_) const
{
    if (!object_ || !object_->transform)
    {
        return playerBottomOffset;
    }
    return AddFloat3(object_->transform->GetWorldPosition(), playerBottomOffset);
}

DirectX::XMFLOAT3 SpecialAttackCameraComponent::CalculateWeightedLookAt(
    const DirectX::XMFLOAT3& playerPosition_,
    const DirectX::XMFLOAT3& targetPosition_) const
{
    // プレイヤーと敵の重み付き中点を作り、近距離でも敵だけを見すぎない注視点にする。
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

DirectX::XMFLOAT3 SpecialAttackCameraComponent::ClampCameraPositionToArena(
    const DirectX::XMFLOAT3& cameraPosition_) const
{
    // 円形アリーナ外へ出たカメラを内側の許容半径へ戻し、壁抜けや外周越しの視点を防ぐ。
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

float SpecialAttackCameraComponent::CalculateWallCameraCandidateScore(
    const DirectX::XMFLOAT3& candidatePosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float fovY_,
    int candidateSide_) const
{
    // 壁際カメラ候補を投影スコアとアリーナ補正量で評価し、現在側には少し保持ボーナスを与える。
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

DirectX::XMFLOAT3 SpecialAttackCameraComponent::CalculateCollisionLookAt(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& baseLookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float compressionRate_,
    float liftRate_) const
{
    // 人型用プロファイルでは、ドラゴン向けの縦補正を候補計算から外す。
    if (!enableCollisionVerticalCorrection)
    {
        return baseLookAtPosition_;
    }
    // 衝突でカメラが近づいたとき、足元や敵が切れない注視点候補を複数作って最も収まりがよいものを選ぶ。
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

DirectX::XMFLOAT3 SpecialAttackCameraComponent::CalculateFramingCameraPosition(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float compressionRate_) const
{
    // 人型用プロファイルでは、ドラゴン向けの縦補正を候補計算から外す。
    if (!enableCollisionVerticalCorrection)
    {
        return cameraPosition_;
    }
    // 縦方向に画面外へ出そうな場合は、注視点から後ろ方向へカメラを少し引いて構図を回復する。
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

float SpecialAttackCameraComponent::CalculateCollisionFov(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& playerTopPosition_,
    const DirectX::XMFLOAT3& playerBottomPosition_,
    const DirectX::XMFLOAT3& targetPosition_,
    float compressionRate_) const
{
    // FOV候補を段階的に試し、画面内収まりを改善しつつ広角になりすぎない値を選ぶ。
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

bool SpecialAttackCameraComponent::ProjectToScreenPosition(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_,
    const DirectX::XMFLOAT3& worldPosition_,
    float fovY_,
    DirectX::XMFLOAT2& screenPosition_) const
{
    // 指定カメラ姿勢でワールド座標を0-1のスクリーン座標へ投影し、候補評価の共通入力にする。
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_)
    {
        return false;
    }

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

float SpecialAttackCameraComponent::CalculateProjectionScore(
    const DirectX::XMFLOAT2& playerTopScreen_,
    const DirectX::XMFLOAT2& playerBottomScreen_,
    const DirectX::XMFLOAT2& targetScreen_,
    bool playerTopVisible_,
    bool playerBottomVisible_,
    bool targetVisible_) const
{
    // プレイヤー上半身、足元、敵が安全枠から外れるほど高スコアにし、候補選択で避けられるようにする。
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

void SpecialAttackCameraComponent::Gui()
{
    ImGui::Text("Tracking: %s", IsTracking() ? "Active" : "Inactive");
    ImGui::DragFloat3("Player Offset", &playerOffset.x, 0.05f);
    ImGui::DragFloat3("Player Bottom Offset", &playerBottomOffset.x, 0.05f);
    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.05f);

    ImGui::Checkbox("Close Range Distance Guard", &enableCloseRangeDistanceGuard);
    ImGui::DragFloat("Close Range Direction Hold Distance", &closeRangeDirectionHoldDistance, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Close Range Orbit Turn Speed", &closeRangeOrbitTurnSpeed, 1.0f, 0.0f, 360.0f);
    ImGui::DragFloat("Close Range Minimum Height", &closeRangeMinimumHeight, 0.05f, 0.0f, 10.0f);
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
    if (ImGui::RadioButton("Right Shoulder", currentSide > 0.0f))
    {
        currentSide = 1.0f;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Left Shoulder", currentSide < 0.0f))
    {
        currentSide = -1.0f;
    }
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
    ImGui::Checkbox("Collision Vertical Correction", &enableCollisionVerticalCorrection);
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
}

void SpecialAttackCameraComponent::Serialize(nlohmann::json& json_) const
{
    json_["playerOffset"] = { playerOffset.x, playerOffset.y, playerOffset.z };
    json_["playerBottomOffset"] =
    {
        playerBottomOffset.x,
        playerBottomOffset.y,
        playerBottomOffset.z
    };
    json_["targetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
    json_["enableCloseRangeDistanceGuard"] = enableCloseRangeDistanceGuard;
    json_["closeRangeDirectionHoldDistance"] = closeRangeDirectionHoldDistance;
    json_["closeRangeOrbitTurnSpeed"] = closeRangeOrbitTurnSpeed;
    json_["closeRangeMinimumHeight"] = closeRangeMinimumHeight;
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
    json_["enableCollisionVerticalCorrection"] = enableCollisionVerticalCorrection;
    json_["collisionLookAtSearchRadius"] = collisionLookAtSearchRadius;
    json_["collisionLookAtInterpolationSpeed"] =
        collisionLookAtInterpolationSpeed;
    json_["collisionFovMaxAddition"] = collisionFovMaxAddition;
    json_["collisionFovInterpolationSpeed"] =
        collisionFovInterpolationSpeed;
    json_["enableCircularArenaCamera"] = enableCircularArenaCamera;
    // 演出途中のScene保存でも、通常戦闘用の設定を侵食後の値で上書きしない。
    const auto& savedArenaCenter_ = hasRuntimeArenaBounds ? originalArenaCenter : arenaCenter;
    json_["arenaCenter"] = { savedArenaCenter_.x, savedArenaCenter_.y, savedArenaCenter_.z };
    json_["arenaRadius"] = hasRuntimeArenaBounds ? originalArenaRadius : arenaRadius;
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
}

void SpecialAttackCameraComponent::Deserialize(const nlohmann::json& json_)
{
    hasRuntimeArenaBounds = false;
    auto loadFloat3_ = [&json_](
        const char* key_,
        DirectX::XMFLOAT3& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() ||
            json_[key_].size() < 3)
        {
            return;
        }
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
    enableCloseRangeDistanceGuard = json_.value("enableCloseRangeDistanceGuard", false);
    closeRangeDirectionHoldDistance = (std::max)(0.0f, json_.value("closeRangeDirectionHoldDistance", 1.5f));
    closeRangeOrbitTurnSpeed = (std::max)(0.0f, json_.value("closeRangeOrbitTurnSpeed", 90.0f));
    closeRangeMinimumHeight = (std::max)(0.0f, json_.value("closeRangeMinimumHeight", 1.2f));
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
    enableCollisionVerticalCorrection = json_.value("enableCollisionVerticalCorrection", true);
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
    minCameraDistance = (std::max)(minCameraDistance, 0.1f);
    maxCameraDistance = (std::max)(maxCameraDistance, minCameraDistance);
    ResetTracking();
}

REGISTER_COMPONENT(SpecialAttackCameraComponent, "SpecialAttackCameraComponent")
