#include "PrecompiledHeader.h"

#include "CameraLockOnComponent.h"

#include "CameraComponent.h"
#include "CameraCollisionComponent.h"
#include "SpecialAttackCameraComponent.h"
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
    // 旧シーンの構図設定は、特殊攻撃カメラをまだ持たない場合だけ移す。
    if (const auto owner_ = GetGameObject(); owner_ && specialAttackCamera.expired() && !legacySpecialAttackSettings.is_null())
    {
        const auto special_ = owner_->GetOrAddComponent<SpecialAttackCameraComponent>();
        special_->Deserialize(legacySpecialAttackSettings);
        if (const auto collision_ = owner_->GetComponent<CameraCollisionComponent>();
            collision_ && collision_->GetLateUpdatePriority() <= special_->GetLateUpdatePriority())
        {
            collision_->SetLateUpdatePriority(special_->GetLateUpdatePriority() + 1);
        }
        specialAttackCamera = special_;
    }
    legacySpecialAttackSettings = nullptr;
    RegisterActions();
}

void CameraLockOnComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("CameraLockOnComponent::LateUpdate");

    // LateUpdateで実カメラを上書きし、他の移動更新が終わった後のプレイヤー/敵位置を基準に構図を作る。
    ResolveReferences();
    if (actionSuppressed)
    {
        return;
    }
    if (const auto special_ = specialAttackCamera.lock(); special_ && special_->IsTracking())
    {
        hasTrackingPose = false;
        return;
    }
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
    const auto previousEnemy_ = lockOnTarget.lock();
    const auto eligibilityTarget_ = lockOnFocusTarget.lock();
    const auto eligibility_ = eligibilityTarget_ ? eligibilityTarget_->GetComponent<LockOnTargetComponent>() : nullptr;
    if (previousEnemy_ && (!previousEnemy_->IsCombatReady() || (eligibility_ && !eligibility_->IsAvailable())))
    {
        ForgetObject(previousEnemy_);
        return;
    }
    // フォーカス部位が無効になった場合は、同じ敵の別LockOnTargetや本体へ自動で戻す。
    if (!ResolveFocusTargetFallback())
    {
        SetLockOnEnabled(false);
        return;
    }
    UpdateLockOnSwitchInput(deltaTime_);
    const auto enemy_ = lockOnTarget.lock();
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
        // ロックオン中はThirdPersonCameraの入力制御を止め、このコンポーネントがカメラ姿勢を所有する。
        thirdPerson_->SetControlSuppressed(true);
        ownsThirdPersonControl = true;
    }
    const LockOnDistanceInfo distanceInfo_ = CalculateLockOnDistanceInfo();

    if (!distanceInfo_.isValid)
    {
        SetLockOnEnabled(false);
        return;
    }

    const DirectX::XMFLOAT3 enemyPosition_ = distanceInfo_.targetPosition;
    const float differenceX_ = distanceInfo_.difference.x;
    const float differenceZ_ = distanceInfo_.difference.z;
    const float horizontalDistance_ = distanceInfo_.horizontalDistance;
    const float targetDistance_ = distanceInfo_.targetDistance;
    currentTargetDistance = targetDistance_;

    if (targetDistance_ > maxDistance)
    {
        // 最大距離を超えたらロックオンを解除し、視点が遠い敵へ引っ張られ続けないようにする。
        SetLockOnEnabled(false);
        return;
    }

    // プレイヤーから敵を見た反対方向を基準に、肩越しの横オフセットを加えた通常カメラ位置を作る。
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
    const DirectX::XMFLOAT3 playerCenter_ = thirdPerson_
        ? AddFloat3(player_->transform->GetWorldPosition(), thirdPerson_->GetTargetOffset())
        : distanceInfo_.playerPosition;
    collisionAnchor = playerCenter_;
    const float tpsDistance_ = thirdPerson_ ? thirdPerson_->GetBaseDistance() : baseDistance;
    const auto orbitDirection_ = NormalizeFloat3(AddFloat3(
        { backDirection_.x * tpsDistance_, cameraHeight, backDirection_.z * tpsDistance_ },
        ScaleFloat3(sideDirection_, sideOffset * currentSide)));
    const auto desiredPosition_ = AddFloat3(playerCenter_, ScaleFloat3(orbitDirection_, tpsDistance_));

    const float focusRate_ = std::clamp(horizontalDistance_ / (std::max)(closeDistance, 0.001f), 0.0f, 1.0f);
    const float weight_ = std::lerp(closeFocusBias, farFocusBias, focusRate_);
    auto focusShift_ = ScaleFloat3(SubtractFloat3(enemyPosition_, playerCenter_), weight_);
    // 大きく離れた敵や空中の敵でもプレイヤーが画面端へ押し出されにくいよう、注視点の移動量を制限する。
    constexpr float MAX_FOCUS_SHIFT_RATIO = 0.5f;
    const float focusShiftLength_ = std::hypot(focusShift_.x, focusShift_.y, focusShift_.z);
    const float maxFocusShift_ = tpsDistance_ * MAX_FOCUS_SHIFT_RATIO;
    if (focusShiftLength_ > maxFocusShift_)
    {
        focusShift_ = ScaleFloat3(focusShift_, maxFocusShift_ / focusShiftLength_);
    }
    auto desiredLookAt_ = AddFloat3(playerCenter_, focusShift_);
    currentBaseCameraPosition = desiredPosition_;
    currentBaseLookAtPosition = desiredLookAt_;
    const float compositionWeight_ = compositionCorrection.weight;
    desiredLookAt_.y += compositionCorrection.lookAtHeightOffset * compositionWeight_;
    desiredLookAt_ = AddFloat3(desiredLookAt_, ScaleFloat3(
        SubtractFloat3(enemyPosition_, playerCenter_),
        compositionCorrection.focusTargetRateOffset * compositionWeight_));

    if (!hasTrackingPose || previousLockOnTarget.lock() != enemy_)
    {
        interpolatedPosition = camera_->GetEye();
        interpolatedLookAt = camera_->GetFocus();
        previousLockOnTarget = enemy_;
        hasTrackingPose = true;
    }
    const float positionRate_ = ExpSmoothingRate(positionInterpolationSpeed, deltaTime_);
    const auto previousOrbit_ = SubtractFloat3(interpolatedPosition, playerCenter_);
    const float previousHorizontal_ = LengthHorizontal(previousOrbit_);
    // 対象の切替でカメラがプレイヤーを横切らないよう、角度と半径を別々に補間する。
    if (previousHorizontal_ > 0.0001f)
    {
        const float previousYaw_ = std::atan2(previousOrbit_.z, previousOrbit_.x);
        const float desiredYaw_ = std::atan2(orbitDirection_.z, orbitDirection_.x);
        const float yaw_ = previousYaw_ + std::remainder(desiredYaw_ - previousYaw_, DirectX::XM_2PI) * positionRate_;
        const float pitch_ = std::lerp(
            std::atan2(previousOrbit_.y, previousHorizontal_),
            std::atan2(orbitDirection_.y, LengthHorizontal(orbitDirection_)), positionRate_);
        const float radius_ = std::lerp(std::hypot(previousHorizontal_, previousOrbit_.y), tpsDistance_, positionRate_);
        const float horizontalRadius_ = radius_ * std::cos(pitch_);
        interpolatedPosition = AddFloat3(playerCenter_,
            { horizontalRadius_ * std::cos(yaw_), radius_ * std::sin(pitch_), horizontalRadius_ * std::sin(yaw_) });
    }
    else
    {
        interpolatedPosition = LerpFloat3(interpolatedPosition, desiredPosition_, positionRate_);
    }
    interpolatedLookAt = LerpFloat3(interpolatedLookAt, desiredLookAt_,
        ExpSmoothingRate(lookAtInterpolationSpeed, deltaTime_));
    ApplyCameraPose(interpolatedPosition, interpolatedLookAt);
    if (thirdPerson_)
    {
        thirdPerson_->SynchronizeViewAngles(interpolatedPosition, playerCenter_);
    }
}

void CameraLockOnComponent::SetLockOnEnabled(bool enabled_)
{
    // 有効化時は実際に追える敵とフォーカス点が揃っている場合だけONにし、解除時は補正状態を初期化する。
    if (enabled_)
    {
        ResolveFocusTargetFallback();
    }
    lockOnEnabled = enabled_ && !lockOnTarget.expired() && !lockOnFocusTarget.expired();
    ResetTracking();
    if (!lockOnEnabled)
    {
        ClearCompositionCorrection();
        lockOnFocusTarget.reset();
        lockOnFocusTargetName.clear();
        ReleaseThirdPersonCamera();
    }
}

void CameraLockOnComponent::SetActionSuppressed(bool suppressed_)
{
    actionSuppressed = suppressed_;
    if (!actionSuppressed)
    {
        ResetTracking();
    }
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

float CameraLockOnComponent::GetWallCameraRate() const
{
    const auto special_ = specialAttackCamera.lock();
    return special_ && special_->IsTracking() ? special_->GetWallCameraRate() : 0.0f;
}

float CameraLockOnComponent::GetCurrentTargetDistance() const
{
    const auto special_ = specialAttackCamera.lock();
    return special_ && special_->IsTracking() ? special_->GetCurrentTargetDistance() : currentTargetDistance;
}

DirectX::XMFLOAT3 CameraLockOnComponent::GetCurrentBaseCameraPosition() const
{
    const auto special_ = specialAttackCamera.lock();
    return special_ && special_->IsTracking() ? special_->GetCurrentBaseCameraPosition() : currentBaseCameraPosition;
}

DirectX::XMFLOAT3 CameraLockOnComponent::GetCurrentBaseLookAtPosition() const
{
    const auto special_ = specialAttackCamera.lock();
    return special_ && special_->IsTracking() ? special_->GetCurrentBaseLookAtPosition() : currentBaseLookAtPosition;
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

void CameraLockOnComponent::ForgetObject(const std::shared_ptr<GameObject>& object_)
{
    if (lockOnTarget.lock() != object_ && lockOnFocusTarget.lock() != object_)
    {
        return;
    }
    lockOnTarget.reset();
    lockOnFocusTarget.reset();
    lockOnTargetName.clear();
    lockOnFocusTargetName.clear();
    SetLockOnEnabled(false);
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
    // 初回ロックオンは画面中央に近く、距離が近く、優先度が高い候補を低スコアとして選ぶ。
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
    // シーン内のLockOnTargetComponentを走査し、距離、正面方向、画面内への投影結果を候補情報として集める。
    std::vector<LockOnCandidate> candidates_;
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager || !playerTarget_ || !playerTarget_->transform)
    {
        return candidates_;
    }

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_)
    {
        return candidates_;
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
        if (!lockTarget_ || !lockTarget_->IsAvailable())
        {
            continue;
        }

        const std::shared_ptr<GameObject> combatOwner_ =
            lockTarget_->GetCombatOwner();
        if (!combatOwner_ || !combatOwner_->transform)
        {
            continue;
        }

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
        if (centerDot_ <= 0.0f)
        {
            continue;
        }

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
            if (overflowX_ > 0.35f || overflowY_ > 0.35f)
            {
                continue;
            }
        }

        candidates_.push_back(lockOnCandidate_);
    }

    return candidates_;
}

bool CameraLockOnComponent::ToggleLockOnForPlayer(
    const std::shared_ptr<GameObject>& playerTarget_)
{
    // プレイヤー指定のトグル入口。OFFなら最良候補を探してからON、ONなら即解除する。
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
    if (!target_)
    {
        return false;
    }
    SetLockOnFocusTarget(target_);
    return true;
}

bool CameraLockOnComponent::SwitchLockOnTargetNext()
{
    // 次/前切替は画面X座標順に並べ、現在フォーカスから循環する形で選び直す。
    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget.lock());
    if (candidates_.empty())
    {
        return false;
    }

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
    if (candidates_.empty())
    {
        return false;
    }

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
    // スティックやマウス方向に近い画面上の候補を探し、中心距離や優先度も加味して切替先を決める。
    const float inputLength_ = std::sqrt(
        inputDirection_.x * inputDirection_.x +
        inputDirection_.y * inputDirection_.y);
    if (inputLength_ <= 0.0001f)
    {
        return nullptr;
    }

    const DirectX::XMFLOAT2 normalizedInput_
    {
        inputDirection_.x / inputLength_,
        inputDirection_.y / inputLength_
    };
    const std::vector<LockOnCandidate> candidates_ =
        CollectLockOnCandidates(playerTarget.lock());
    const std::shared_ptr<GameObject> currentFocusTarget_ =
        lockOnFocusTarget.lock();
    if (candidates_.empty() || !currentFocusTarget_)
    {
        return nullptr;
    }

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
    if (!foundCurrentScreen_)
    {
        return nullptr;
    }

    LockOnCandidate bestCandidate_{};
    float bestScore_ = -(std::numeric_limits<float>::max)();
    for (LockOnCandidate candidate_ : candidates_)
    {
        if (candidate_.focusTarget.lock() == currentFocusTarget_)
        {
            continue;
        }

        const DirectX::XMFLOAT2 screenDifference_
        {
            candidate_.screenPosition.x - currentScreen_.x,
            candidate_.screenPosition.y - currentScreen_.y
        };
        const float screenDistance_ = std::sqrt(
            screenDifference_.x * screenDifference_.x +
            screenDifference_.y * screenDifference_.y);
        if (screenDistance_ <= 0.0001f)
        {
            continue;
        }

        const DirectX::XMFLOAT2 candidateDirection_
        {
            screenDifference_.x / screenDistance_,
            screenDifference_.y / screenDistance_
        };
        const float directionDot_ =
            normalizedInput_.x * candidateDirection_.x +
            normalizedInput_.y * candidateDirection_.y;
        if (directionDot_ < directionalDotThreshold)
        {
            continue;
        }

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
    if (!combatOwner_ || !focusTarget_)
    {
        return false;
    }

    lockOnTarget = combatOwner_;
    lockOnTargetName = combatOwner_->GetName();
    lockOnFocusTarget = focusTarget_;
    lockOnFocusTargetName = focusTarget_->GetName();
    debugLastSwitchScore = candidate_.score;
    return true;
}

bool CameraLockOnComponent::ResolveFocusTargetFallback()
{
    // 現在のフォーカスが消えた場合でも、同じ戦闘オーナーに属する別ターゲットへ自然に戻す。
    const std::shared_ptr<GameObject> combatOwner_ = lockOnTarget.lock();
    if (!combatOwner_)
    {
        return false;
    }

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
        if (candidate_.combatOwner.lock() != combatOwner_)
        {
            continue;
        }
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
    if (hasFallback_)
    {
        return ApplyLockOnCandidate(fallbackCandidate_);
    }

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
    const auto owner_ = GetGameObject();
    const auto gameMode_ = owner_ ? owner_->GetManager().GetGameModeManager() : nullptr;
    // RETURN中は通常構図を計算しても、手動の対象切り替え入力は受け付けない。
    if (gameMode_ && gameMode_->IsPlayerControlLocked())
    {
        return;
    }
    // キーボード、ゲームパッド、マウス、ホイールを1つの切替方向へまとめ、ラッチとクールダウンで多重入力を抑える。
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

    const float wheelAxis_ = MouseInput::Instance().IsCursorLocked()
        ? KeyConfig::Instance().GetRawAxis("LockOn_SwitchWheel") : 0.0f;
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
    if (keyConfig_.ActionPressedIn("LockOn_SwitchLeft"))
    {
        input_.x -= 1.0f;
    }
    if (keyConfig_.ActionPressedIn("LockOn_SwitchRight"))
    {
        input_.x += 1.0f;
    }
    if (keyConfig_.ActionPressedIn("LockOn_SwitchUp"))
    {
        input_.y -= 1.0f;
    }
    if (keyConfig_.ActionPressedIn("LockOn_SwitchDown"))
    {
        input_.y += 1.0f;
    }
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
    if (!MouseInput::Instance().IsCursorLocked())
    {
        accumulatedMouseDelta = {};
        debugMouseAccumulatedDelta = {};
        mouseAccumulationTimer = 0.0f;
        return {};
    }
    // マウス移動は一瞬のデルタだけではなく短時間蓄積し、しきい値を超えた軸方向をターゲット切替に使う。
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
    // プロジェクト側で未設定の場合だけ、ロックオン切替用の標準入力を登録しておく。
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

void CameraLockOnComponent::ResolveReferences()
{
    // シリアライズされた名前や同一GameObject上のコンポーネントから、弱参照が切れた分だけ再解決する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    if (camera.expired())
    {
        camera = owner_->GetComponent<CameraComponent>();
    }
    if (specialAttackCamera.expired())
    {
        specialAttackCamera = owner_->GetComponent<SpecialAttackCameraComponent>();
    }
    if (thirdPersonCamera.expired())
    {
        thirdPersonCamera = owner_->GetComponent<ThirdPersonCameraComponent>();
    }
    if (!owner_->manager)
    {
        return;
    }
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
    // AnimationイベントやActionRegistryからロックオン制御を呼べるよう、カメラカテゴリへ操作を登録する。
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
    hasCloseRangeBackDirection = false;
    // ターゲットや有効状態が変わったとき、補間履歴と入力ラッチをまとめてリセットする。
    hasTrackingPose = false;
    previousLockOnTarget.reset();
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
        if (!ownsThirdPersonControl || actionSuppressed)
        {
            return;
        }
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

DirectX::XMFLOAT3 CameraLockOnComponent::GetFocusPosition(
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

bool CameraLockOnComponent::ProjectToScreenPosition(
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

CameraLockOnComponent::LockOnDistanceInfo
CameraLockOnComponent::CalculateLockOnDistanceInfo() const
{
    // LateUpdateとデバッグ表示の両方で使う、プレイヤーと戦闘対象の距離情報を一箇所で計算する。
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
    // 調整中に必要な距離、画面内位置、補正ON/OFFをInspectorへまとめて表示する。
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
    // ロックオン挙動の主要パラメータをInspectorから調整できるようにする。
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
    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.05f);

    DrawLockOnDebugGui();


    ImGui::DragFloat("Max Distance", &maxDistance, 0.1f, 0.1f, 200.0f);
    ImGui::Checkbox("Close Range Distance Guard", &enableCloseRangeDistanceGuard);
    ImGui::DragFloat("Close Range Direction Hold Distance", &closeRangeDirectionHoldDistance, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("Close Range Orbit Turn Speed", &closeRangeOrbitTurnSpeed, 1.0f, 0.0f, 360.0f);
    ImGui::DragFloat("Fallback Distance", &baseDistance, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Camera Height", &cameraHeight, 0.05f, -20.0f, 50.0f);
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
    ImGui::TextDisabled(
        "Actions: LockOn_Enable / Disable / Toggle / SwitchSide / SwitchLeft / SwitchRight / SwitchUp / SwitchDown / SwitchNext / SwitchPrevious");
}

void CameraLockOnComponent::Serialize(nlohmann::json& json_) const
{
    if (runtimeSettingsActive)
    {
        json_ = originalRuntimeSettings;
        return;
    }
    json_ = legacySpecialAttackSettings.is_object() ? legacySpecialAttackSettings : nlohmann::json::object();
    json_["playerTargetName"] = playerTargetName;
    json_["lockOnTargetName"] = lockOnTargetName;
    json_["lockOnFocusTargetName"] = lockOnFocusTargetName;
    json_["lockOnEnabled"] = lockOnEnabled;
    json_["playerOffset"] = { playerOffset.x, playerOffset.y, playerOffset.z };
    json_["targetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
    json_["maxDistance"] = maxDistance;
    json_["enableCloseRangeDistanceGuard"] = enableCloseRangeDistanceGuard;
    json_["closeRangeDirectionHoldDistance"] = closeRangeDirectionHoldDistance;
    json_["closeRangeOrbitTurnSpeed"] = closeRangeOrbitTurnSpeed;
    json_["cameraHeight"] = cameraHeight;
    json_["baseDistance"] = baseDistance;
    json_["sideOffset"] = sideOffset;
    json_["currentSide"] = currentSide;
    json_["closeDistance"] = closeDistance;
    json_["closeFocusBias"] = closeFocusBias;
    json_["farFocusBias"] = farFocusBias;
    json_["positionInterpolationSpeed"] = positionInterpolationSpeed;
    json_["lookAtInterpolationSpeed"] = lookAtInterpolationSpeed;
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
    runtimeSettingsActive = false;
    originalRuntimeSettings = nlohmann::json{};
    if (!IsStarted())
    {
        legacySpecialAttackSettings = json_.contains("distanceScale") ? json_ : nlohmann::json();
    }
    // 設定で対象名を切り替えた際、旧ドラゴンの弱参照を再利用しない。
    playerTarget.reset();
    lockOnTarget.reset();
    lockOnFocusTarget.reset();
    // 旧データや手編集データでも壊れにくいよう、既定値を入れながら読み込み後に範囲をクランプする。
    playerTargetName = json_.value("playerTargetName", std::string{});
    lockOnTargetName = json_.value("lockOnTargetName", std::string{});
    lockOnFocusTargetName = json_.value("lockOnFocusTargetName", std::string{});
    lockOnEnabled = json_.value("lockOnEnabled", false);
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
    loadFloat3_("targetOffset", targetOffset);
    maxDistance = json_.value("maxDistance", 28.0f);
    enableCloseRangeDistanceGuard = json_.value("enableCloseRangeDistanceGuard", false);
    closeRangeDirectionHoldDistance = (std::max)(0.0f, json_.value("closeRangeDirectionHoldDistance", 1.5f));
    closeRangeOrbitTurnSpeed = (std::max)(0.0f, json_.value("closeRangeOrbitTurnSpeed", 90.0f));
    baseDistance = (std::max)(0.1f, json_.value("baseDistance", 7.0f));
    cameraHeight = json_.value("cameraHeight", 2.2f);
    sideOffset = json_.value("sideOffset", 1.25f);
    currentSide = json_.value("currentSide", 1.0f) >= 0.0f ? 1.0f : -1.0f;
    closeDistance = json_.value("closeDistance", 8.0f);
    closeFocusBias = json_.value("closeFocusBias", 0.28f);
    farFocusBias = json_.value("farFocusBias", 0.5f);
    positionInterpolationSpeed = json_.value("positionInterpolationSpeed", 10.0f);
    lookAtInterpolationSpeed = json_.value("lookAtInterpolationSpeed", 12.5f);
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

// 元の設定は一度だけ保存し、第二形態のカメラ設定を実行時へ適用する。
void CameraLockOnComponent::ApplyRuntimeSettings(const nlohmann::json& settings_)
{
    BS_PROFILE_SCOPE("CameraLockOnComponent::ApplyRuntimeSettings");
    nlohmann::json original_;
    Serialize(original_);
    Deserialize(settings_);
    ClearCompositionCorrection();
    originalRuntimeSettings = std::move(original_);
    runtimeSettingsActive = true;
}

void CameraLockOnComponent::ClearRuntimeSettings()
{
    if (!runtimeSettingsActive)
    {
        return;
    }
    const auto original_ = originalRuntimeSettings;
    Deserialize(original_);
}
