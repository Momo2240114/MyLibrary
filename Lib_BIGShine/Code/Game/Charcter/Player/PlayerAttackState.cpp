#include "PrecompiledHeader.h"

#include "PlayerAttackState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Physics/CapsuleCollider.h"
#include "PlayerControllerComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float MINIMUM_HORIZONTAL_LENGTH_SQUARED = 0.000001f;
    constexpr float RADIANS_TO_DEGREES = 180.0f / DirectX::XM_PI;
    constexpr float DEGREES_TO_RADIANS = DirectX::XM_PI / 180.0f;

    float GetWorldCapsuleRadius(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_ || !object_->transform) return 0.0f;

        const std::shared_ptr<CapsuleCollider> capsule_ =
            object_->GetComponent<CapsuleCollider>();
        if (!capsule_) return 0.0f;

        const DirectX::XMFLOAT3 scale_ = object_->transform->GetWorldScale();
        const float horizontalScale_ =
            (std::max)(std::abs(scale_.x), std::abs(scale_.z));
        return capsule_->radius * capsule_->shapeSizeScale * horizontalScale_;
    }

    bool NormalizeHorizontal(DirectX::XMFLOAT3& direction_)
    {
        direction_.y = 0.0f;
        const float lengthSquared_ =
            direction_.x * direction_.x + direction_.z * direction_.z;
        if (lengthSquared_ <= MINIMUM_HORIZONTAL_LENGTH_SQUARED) return false;

        const float inverseLength_ = 1.0f / std::sqrt(lengthSquared_);
        direction_.x *= inverseLength_;
        direction_.z *= inverseLength_;
        return true;
    }

    float DotHorizontal(
        const DirectX::XMFLOAT3& a_,
        const DirectX::XMFLOAT3& b_)
    {
        return a_.x * b_.x + a_.z * b_.z;
    }

    float GetYawDegrees(const DirectX::XMFLOAT3& direction_)
    {
        return std::atan2(direction_.x, direction_.z) * RADIANS_TO_DEGREES;
    }

    float NormalizeAngleDegrees(float angle_)
    {
        while (angle_ > 180.0f) angle_ -= 360.0f;
        while (angle_ < -180.0f) angle_ += 360.0f;
        return angle_;
    }

    DirectX::XMFLOAT3 DirectionFromYawDegrees(float yawDegrees_)
    {
        const float radians_ = yawDegrees_ * DEGREES_TO_RADIANS;
        return { std::sin(radians_), 0.0f, std::cos(radians_) };
    }

    float SmoothStep(float value_)
    {
        const float clamped_ = std::clamp(value_, 0.0f, 1.0f);
        return clamped_ * clamped_ * (3.0f - 2.0f * clamped_);
    }

    float CalculateFacingAssistWeight(
        float normalizedTime_,
        const PlayerAttackAssistSettings& settings_)
    {
        const float start_ = std::clamp(settings_.facingAssistStartNormalizedTime, 0.0f, 1.0f);
        const float fullEnd_ = std::clamp(settings_.facingAssistFullStrengthEndNormalizedTime, start_, 1.0f);
        const float end_ = std::clamp(settings_.facingAssistEndNormalizedTime, fullEnd_, 1.0f);

        if (normalizedTime_ < start_ || normalizedTime_ >= end_) return 0.0f;
        if (normalizedTime_ <= fullEnd_)
        {
            const float duration_ = (std::max)(fullEnd_ - start_, 0.0001f);
            return SmoothStep((normalizedTime_ - start_) / duration_);
        }

        const float duration_ = (std::max)(end_ - fullEnd_, 0.0001f);
        return 1.0f - SmoothStep((normalizedTime_ - fullEnd_) / duration_);
    }
}

PlayerAttackState::PlayerAttackState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

void PlayerAttackState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    // 攻撃開始ごとに補正状態をリセットし、今回の攻撃だけで向き/位置補正を計算する。
    attackElapsedTime = 0.0f;
    accumulatedFacingCorrectionDegrees = 0.0f;
    hasLockedAssistDirection = false;
    attackAssistTargetResolved = false;
    attackAssistTarget.reset();
    player_->EnterAction(action);
    const bool isStrongCharge_ =
        action == PLAYER_ACTION::STRONG_CHARGE_ATTACK ||
        action == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK;
    const bool isAirAttack_ =
        action == PLAYER_ACTION::AIR_WEAK_ATTACK ||
        action == PLAYER_ACTION::AIR_STRONG_ATTACK ||
        action == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK;

    if (action == PLAYER_ACTION::AIR_WEAK_ATTACK &&
        player_->GetComboIndex() == 0)
    {
        if (std::shared_ptr<CharacterMovementComponent> movement_ =
            GetComponent<CharacterMovementComponent>())
        {
            movement_->StopHorizontalMovementImmediately();
        }
    }

    if (isStrongCharge_)
    {
        // 溜め強攻撃は開始時の入力方向、入力が無い場合はカメラ前方へ踏み込ませる。
        DirectX::XMFLOAT3 direction_ = player_->GetMoveInput();
        if (direction_.x == 0.0f && direction_.z == 0.0f)
        {
            direction_ = player_->GetCameraForwardDirection();
        }
        player_->SetFacingDirection(direction_);
        player_->SetHorizontalVelocity(direction_, player_->strongChargeAttackSpeed);
    }
    else if (!isAirAttack_)
    {
        player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    }

    ResolveAttackAssistTarget();
}

void PlayerAttackState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    // 攻撃中も入力をBlackboardへ流し、コンボ分岐やキャンセル判定に使える状態を保つ。
    attackElapsedTime += (std::max)(elapsedTime_, 0.0f);
    ApplyAttackAssist(elapsedTime_);
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

void PlayerAttackState::Exit()
{
    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
        if (!player_->GetAttackAssistSettings().preserveTargetAcrossCombo)
        {
            player_->ClearAttackAssistTarget();
        }
    }
}

std::shared_ptr<GameObject> PlayerAttackState::FindAssistTarget()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!owner_ || !owner_->transform || !player_) return nullptr;

    // ロックオン、入力方向、カメラ正面、距離をスコア化して攻撃補正先を選ぶ。
    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const float maximumDistanceSquared_ = settings_.searchDistance * settings_.searchDistance;

    const auto isValidTarget_ = [&](const std::shared_ptr<GameObject>& target_)
    {
        if (!target_ || target_.get() == owner_.get() || !target_->transform) return false;
        if (target_->layer != LayerType::Enemy) return false;

        const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();
        const float differenceX_ = targetPosition_.x - ownerPosition_.x;
        const float differenceZ_ = targetPosition_.z - ownerPosition_.z;
        const float distanceSquared_ = differenceX_ * differenceX_ + differenceZ_ * differenceZ_;
        return
            distanceSquared_ <= maximumDistanceSquared_ &&
            distanceSquared_ > MINIMUM_HORIZONTAL_LENGTH_SQUARED;
    };

    const std::shared_ptr<CameraLockOnComponent> lockOn_ = ResolveCameraLockOnComponent();
    const bool isLockOn_ = lockOn_ && lockOn_->IsLockOnEnabled();
    if (isLockOn_)
    {
        const std::shared_ptr<GameObject> lockOnTarget_ = lockOn_->GetLockOnTarget();
        if (isValidTarget_(lockOnTarget_)) return lockOnTarget_;
    }

    std::shared_ptr<CameraComponent> camera_;
    if (CameraManager* cameraManager_ = owner_->GetManager().GetCameraManager())
    {
        camera_ = cameraManager_->GetActiveCamera();
    }

    DirectX::XMFLOAT3 moveInput_ = player_->GetMoveInput();
    const bool hasMoveInput_ =
        settings_.preferMoveInputDirection &&
        NormalizeHorizontal(moveInput_) &&
        std::sqrt(
            player_->GetMoveInput().x * player_->GetMoveInput().x +
            player_->GetMoveInput().z * player_->GetMoveInput().z) >=
            (std::max)(settings_.minimumInputMagnitude, 0.0f);

    DirectX::XMFLOAT3 cameraForward_{ 0.0f, 0.0f, 1.0f };
    bool hasCameraForward_ = false;
    if (camera_)
    {
        cameraForward_ = camera_->GetFront();
        hasCameraForward_ = NormalizeHorizontal(cameraForward_);
    }

    DirectX::XMFLOAT3 ownerForward_
    {
        owner_->transform->GetFront().x,
        0.0f,
        owner_->transform->GetFront().z
    };
    if (!NormalizeHorizontal(ownerForward_))
    {
        ownerForward_ = { 0.0f, 0.0f, 1.0f };
    }

    const float maximumAngleDegrees_ = isLockOn_
        ? settings_.lockOnMaximumTargetAngleDegrees
        : settings_.nonLockOnMaximumTargetAngleDegrees;
    const float targetThreshold_ = std::cos(
        std::clamp(maximumAngleDegrees_, 0.0f, 180.0f) * DEGREES_TO_RADIANS);
    const float centerThreshold_ = std::cos(
        std::clamp(settings_.cameraCenterAngle, 0.0f, 180.0f) * DEGREES_TO_RADIANS);
    const DirectX::XMFLOAT3 primaryDirection_ = hasMoveInput_
        ? moveInput_
        : (settings_.preferCameraForwardDirection && hasCameraForward_
            ? cameraForward_
            : ownerForward_);

    std::shared_ptr<GameObject> bestTarget_;
    std::shared_ptr<GameObject> nearestTarget_;
    float bestScore_ = -(std::numeric_limits<float>::max)();
    float nearestDistanceSquared_ = (std::numeric_limits<float>::max)();

    for (const std::shared_ptr<GameObject>& candidate_ : owner_->GetManager().GetObjects())
    {
        if (!isValidTarget_(candidate_)) continue;

        const DirectX::XMFLOAT3 candidatePosition_ = candidate_->transform->GetWorldPosition();
        const float differenceX_ = candidatePosition_.x - ownerPosition_.x;
        const float differenceZ_ = candidatePosition_.z - ownerPosition_.z;
        const float distanceSquared_ = differenceX_ * differenceX_ + differenceZ_ * differenceZ_;

        DirectX::XMFLOAT3 targetDirection_{ differenceX_, 0.0f, differenceZ_ };
        if (!NormalizeHorizontal(targetDirection_)) continue;

        const float primaryDot_ = DotHorizontal(targetDirection_, primaryDirection_);
        if (primaryDot_ < targetThreshold_) continue;

        if (distanceSquared_ < nearestDistanceSquared_)
        {
            nearestDistanceSquared_ = distanceSquared_;
            nearestTarget_ = candidate_;
        }

        const float cameraDot_ = hasCameraForward_
            ? DotHorizontal(targetDirection_, cameraForward_)
            : primaryDot_;
        const float ownerDot_ = DotHorizontal(targetDirection_, ownerForward_);
        const float distance_ = std::sqrt(distanceSquared_);
        const float distanceScore_ = 1.0f - std::clamp(distance_ / (std::max)(settings_.searchDistance, 0.001f), 0.0f, 1.0f);
        const float centerScore_ = hasCameraForward_ && cameraDot_ >= centerThreshold_
            ? 0.2f
            : 0.0f;
        const float score_ =
            primaryDot_ * 0.55f +
            cameraDot_ * 0.2f +
            ownerDot_ * 0.15f +
            distanceScore_ * 0.1f +
            centerScore_;

        if (score_ > bestScore_)
        {
            bestScore_ = score_;
            bestTarget_ = candidate_;
        }
    }

    if (bestTarget_) return bestTarget_;
    return settings_.nonLockOnUseNearestFallback ? nearestTarget_ : nullptr;
}

void PlayerAttackState::ResolveAttackAssistTarget()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!owner_ || !owner_->transform || !player_) return;

    // コンボ中の補正先維持や入力方向優先をここで決め、攻撃中に使う基準方向を固定する。
    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();

    DirectX::XMFLOAT3 startForward_
    {
        owner_->transform->GetFront().x,
        0.0f,
        owner_->transform->GetFront().z
    };
    if (!NormalizeHorizontal(startForward_))
    {
        startForward_ = { 0.0f, 0.0f, 1.0f };
    }

    DirectX::XMFLOAT3 moveInput_ = player_->GetMoveInput();
    const float moveInputLength_ = std::sqrt(
        moveInput_.x * moveInput_.x + moveInput_.z * moveInput_.z);
    if (settings_.preferMoveInputDirection &&
        moveInputLength_ >= (std::max)(settings_.minimumInputMagnitude, 0.0f) &&
        NormalizeHorizontal(moveInput_))
    {
        startForward_ = moveInput_;
    }

    attackStartForward = startForward_;
    lockedAssistDirection = startForward_;
    hasLockedAssistDirection = true;

    std::shared_ptr<GameObject> target_;
    if (settings_.preserveTargetAcrossCombo)
    {
        const std::shared_ptr<GameObject> preservedTarget_ =
            player_->GetAttackAssistTarget();
        if (preservedTarget_ &&
            preservedTarget_->transform &&
            preservedTarget_->layer == LayerType::Enemy)
        {
            target_ = preservedTarget_;
        }
    }

    if (settings_.lockTargetOnAttackEnter)
    {
        if (!target_)
        {
            target_ = FindAssistTarget();
        }
    }

    attackAssistTarget = target_;
    attackAssistTargetResolved = true;
    player_->SetAttackAssistTarget(target_);

    if (target_ && target_->transform)
    {
        const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
        const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();
        DirectX::XMFLOAT3 targetDirection_
        {
            targetPosition_.x - ownerPosition_.x,
            0.0f,
            targetPosition_.z - ownerPosition_.z
        };
        if (NormalizeHorizontal(targetDirection_))
        {
            lockedAssistDirection = targetDirection_;
        }
    }
}

void PlayerAttackState::ApplyAttackAssist(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform) return;

    // 攻撃中の見た目と手触りを補助するため、向き補正と位置補正を分けて適用する。
    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    if (!settings_.enabled || settings_.searchDistance <= 0.0f) return;

    if (!attackAssistTargetResolved || settings_.allowRetargetDuringAttack)
    {
        ResolveAttackAssistTarget();
    }

    const std::shared_ptr<GameObject> target_ = attackAssistTarget.lock();
    if (!target_ || !target_->transform)
    {
        if (settings_.stopAssistWhenTargetInvalid)
        {
            player_->ClearAttackAssistTarget();
            attackAssistTarget.reset();
        }
        return;
    }

    ApplyAttackFacingAssist(target_, elapsedTime_);
    ApplyAttackPositionAssist(target_, elapsedTime_);
}


void PlayerAttackState::ApplyAttackFacingAssist(
    const std::shared_ptr<GameObject>& target_,
    float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform || !target_ || !target_->transform) return;

    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    if (settings_.strength <= 0.0f || settings_.turnSpeed <= 0.0f) return;

    const float normalizedTime_ = GetAttackNormalizedTime();
    const float assistWeight_ =
        CalculateFacingAssistWeight(normalizedTime_, settings_) * settings_.strength;
    if (assistWeight_ <= 0.0f) return;

    // アニメーション時間に応じて補正量を変え、攻撃終盤で不自然に回り続けないようにする。
    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();

    DirectX::XMFLOAT3 desiredDirection_
    {
        targetPosition_.x - ownerPosition_.x,
        0.0f,
        targetPosition_.z - ownerPosition_.z
    };

    if (!NormalizeHorizontal(desiredDirection_))
    {
        desiredDirection_ = lockedAssistDirection;
        if (!NormalizeHorizontal(desiredDirection_)) return;
    }

    const Vector3 currentForwardVector_ = owner_->transform->GetFront();

    DirectX::XMFLOAT3 currentDirection_
    {
        currentForwardVector_.x,
        0.0f,
        currentForwardVector_.z
    };

    if (!NormalizeHorizontal(currentDirection_))
    {
        currentDirection_ = attackStartForward;
        if (!NormalizeHorizontal(currentDirection_))
        {
            currentDirection_ = desiredDirection_;
        }
    }

    const float currentYaw_ = GetYawDegrees(currentDirection_);
    const float desiredYaw_ = GetYawDegrees(desiredDirection_);
    float yawDifference_ = NormalizeAngleDegrees(desiredYaw_ - currentYaw_);
    if (std::abs(yawDifference_) <= 0.001f) return;

    const float remainingTotalDegrees_ =
        (std::max)(settings_.maximumTotalRotationDegrees, 0.0f) -
        accumulatedFacingCorrectionDegrees;
    if (remainingTotalDegrees_ <= 0.0f) return;

    const float maxDegreesPerSecond_ = (std::max)(settings_.turnSpeed * 36.0f, 0.0f);
    const float frameLimitDegrees_ =
        maxDegreesPerSecond_ * (std::max)(elapsedTime_, 0.0f) * assistWeight_;
    const float allowedDegrees_ =
        (std::min)(remainingTotalDegrees_, frameLimitDegrees_);
    if (allowedDegrees_ <= 0.0f) return;

    const float appliedYawDifference_ =
        std::clamp(yawDifference_, -allowedDegrees_, allowedDegrees_);
    const float correctedYaw_ = currentYaw_ + appliedYawDifference_;
    DirectX::XMFLOAT3 correctedDirection_ = DirectionFromYawDegrees(correctedYaw_);

    if (!NormalizeHorizontal(correctedDirection_))
    {
        return;
    }

    accumulatedFacingCorrectionDegrees += std::abs(appliedYawDifference_);
    player_->SetFacingDirection(correctedDirection_);
}

void PlayerAttackState::ApplyAttackPositionAssist(
    const std::shared_ptr<GameObject>& target_,
    float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform || !target_ || !target_->transform) return;

    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();

    if (settings_.positionStrength <= 0.0f ||
        settings_.positionSpeed <= 0.0f)
    {
        return;
    }

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();

    DirectX::XMFLOAT3 toTarget_
    {
        targetPosition_.x - ownerPosition_.x,
        0.0f,
        targetPosition_.z - ownerPosition_.z
    };

    const float distanceSquared_ =
        toTarget_.x * toTarget_.x +
        toTarget_.z * toTarget_.z;

    if (distanceSquared_ <= 0.000001f) return;

    const float distance_ = std::sqrt(distanceSquared_);
    const float colliderStopDistance_ =
        GetWorldCapsuleRadius(owner_) + GetWorldCapsuleRadius(target_) + 0.05f;
    const float stopDistance_ =
        (std::max)((std::max)(settings_.targetStopDistance, 0.0f), colliderStopDistance_);

    if (distance_ <= stopDistance_)
    {
        return;
    }

    const float inverseDistance_ = 1.0f / distance_;
    toTarget_.x *= inverseDistance_;
    toTarget_.z *= inverseDistance_;

    const float availableDistance_ = distance_ - stopDistance_;
    const float maxCorrectionSpeed_ = (std::max)(settings_.maxPositionCorrection, 0.0f);

    // 近づきすぎない停止距離を残しつつ、届きにくい攻撃だけ軽く吸い付かせる。
    float assistSpeed_ =
        (std::min)(availableDistance_, maxCorrectionSpeed_) *
        settings_.positionSpeed *
        settings_.positionStrength;
    if (elapsedTime_ > 0.000001f)
    {
        assistSpeed_ = (std::min)(assistSpeed_, availableDistance_ / elapsedTime_);
    }

    player_->AddHorizontalVelocityAssist(toTarget_, assistSpeed_);
}

float PlayerAttackState::GetAttackNormalizedTime()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (owner_)
    {
        const std::shared_ptr<AnimationControllerComponent> animationController_ =
            owner_->GetComponent<AnimationControllerComponent>();
        if (animationController_)
        {
            const float duration_ = animationController_->GetCurrentSampleDuration();
            if (duration_ > 0.000001f)
            {
                return std::clamp(
                    animationController_->GetAnimatorTime() / duration_,
                    0.0f,
                    1.0f);
            }
        }
    }

    return std::clamp(attackElapsedTime, 0.0f, 1.0f);
}

std::shared_ptr<CameraLockOnComponent> PlayerAttackState::ResolveCameraLockOnComponent()
{
    if (const std::shared_ptr<CameraLockOnComponent> cached_ = cameraLockOnComponent.lock())
    {
        return cached_;
    }

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return nullptr;

    for (const std::shared_ptr<GameObject>& object_ : owner_->GetManager().GetObjects())
    {
        if (!object_) continue;
        const std::shared_ptr<CameraLockOnComponent> component_ =
            object_->GetComponent<CameraLockOnComponent>();
        if (!component_) continue;

        cameraLockOnComponent = component_;
        return component_;
    }

    return nullptr;
}
