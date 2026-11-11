#include "PrecompiledHeader.h"

#include "PlayerAttackState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Physics/CapsuleCollider.h"
#include "Engine/Physics/PhysXSystem.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateRuntime.h"
#include "PlayerControllerComponent.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

namespace
{
    constexpr float MINIMUM_HORIZONTAL_LENGTH_SQUARED = 0.000001f;
    constexpr float RADIANS_TO_DEGREES = 180.0f / DirectX::XM_PI;
    constexpr float DEGREES_TO_RADIANS = DirectX::XM_PI / 180.0f;
    constexpr float ATTACK_ASSIST_VERTICAL_DISTANCE_SCALE = 4.0f;

    // 部位HurtBoxも確認し、受け側のない表示専用Objectや明示無敵の敵へ吸着しない。
    bool HasActiveAttackReceiver(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_ || object_->IsPendingDestroy()) return false;
        for (const auto& hurtBox_ : object_->GetComponents<HurtBox>())
            if (hurtBox_->IsEnabled() && !hurtBox_->IsInvincible()) return true;
        if (object_->transform)
            for (const auto& child_ : object_->transform->children)
                if (child_ && HasActiveAttackReceiver(child_->gameObject.lock())) return true;
        return false;
    }

    // ObjectのWorldスケールを考慮したCapsule半径を取得し、接近補正の停止距離に使う。
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

    // XZ平面の方向だけを正規化し、長さが足りない場合はfalseを返す。
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

    // XZ平面だけで内積を計算する。
    float DotHorizontal(
        const DirectX::XMFLOAT3& a_,
        const DirectX::XMFLOAT3& b_)
    {
        return a_.x * b_.x + a_.z * b_.z;
    }

    // 高い部位を優先しすぎないよう、高さ差だけ重くした距離を攻撃補正Target選択に使う。
    float CalculateAttackAssistDistanceSquared(
        const DirectX::XMFLOAT3& ownerPosition_,
        const DirectX::XMFLOAT3& targetPosition_)
    {
        const float differenceX_ = targetPosition_.x - ownerPosition_.x;
        const float differenceY_ =
            (targetPosition_.y - ownerPosition_.y) * ATTACK_ASSIST_VERTICAL_DISTANCE_SCALE;
        const float differenceZ_ = targetPosition_.z - ownerPosition_.z;
        return
            differenceX_ * differenceX_ +
            differenceY_ * differenceY_ +
            differenceZ_ * differenceZ_;
    }

    // XZ方向ベクトルをYaw角度へ変換する。
    float GetYawDegrees(const DirectX::XMFLOAT3& direction_)
    {
        return std::atan2(direction_.x, direction_.z) * RADIANS_TO_DEGREES;
    }

    // 角度差を-180から180の範囲へ丸める。
    float NormalizeAngleDegrees(float angle_)
    {
        while (angle_ > 180.0f) angle_ -= 360.0f;
        while (angle_ < -180.0f) angle_ += 360.0f;
        return angle_;
    }

    // Yaw角度からXZ平面の方向ベクトルを作る。
    DirectX::XMFLOAT3 DirectionFromYawDegrees(float yawDegrees_)
    {
        const float radians_ = yawDegrees_ * DEGREES_TO_RADIANS;
        return { std::sin(radians_), 0.0f, std::cos(radians_) };
    }

    // 攻撃補正の開始・終了を滑らかにする補間曲線。
    float SmoothStep(float value_)
    {
        const float clamped_ = std::clamp(value_, 0.0f, 1.0f);
        return clamped_ * clamped_ * (3.0f - 2.0f * clamped_);
    }

    // Animation正規化時間から、そのフレームで使う向き補正の重みを計算する。
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

// 攻撃Action種別を受け取り、同じState実装で弱/強/空中攻撃を扱う。
PlayerAttackState::PlayerAttackState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

// 攻撃開始時に補正状態を初期化し、必要なら突進方向や停止速度を設定する。
void PlayerAttackState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    // 攻撃開始ごとに補正状態をリセットし、今回の攻撃だけで向き/位置補正を計算する。
    weakMotionWarpStarted = false;
    weakMotionWarpPresetName.clear();
    weakPlannedRootMotion = {};
    weakWarpEndNormalizedTime = 0.0f;
    attackElapsedTime = 0.0f;
    accumulatedFacingCorrectionDegrees = 0.0f;
    hasLockedAssistDirection = false;
    attackAssistTargetResolved = false;
    attackAssistTarget.reset();
    counterMotionWarpActive = false;
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
        // 溜め強攻撃は開始時の入力方向、入力がない場合はカメラ前方へ踏み込ませる。
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

    if (action == PLAYER_ACTION::WEAK_ATTACK) PrepareWeakComboMotionWarp();
    ResolveAttackAssistTarget();
    if (action == PLAYER_ACTION::WEAK_ATTACK)
    {
        // 対象なしでも、明確な入力方向への空振りは維持する。
        const auto direction_ = player_->GetMoveInput();
        if (!attackAssistTarget.lock() && direction_.x * direction_.x + direction_.z * direction_.z > 0.01f)
            player_->SetFacingDirection(direction_);
    }
    if (action == PLAYER_ACTION::COUNTER_ATTACK)
    {
        StartCounterMotionWarp();
    }
}

// 各段の先頭イベントから開始し、コンボ遷移直後の旧クリップへ補正を適用しない。
void PlayerAttackState::NotifyWeakAnimationStarted()
{
    if (action == PLAYER_ACTION::WEAK_ATTACK) StartWeakComboMotionWarp();
}

// 攻撃中も入力値をBlackboardへ流し、補正とキャンセル分岐を更新する。
void PlayerAttackState::Execute(float elapsedTime_)
{
    BS_PROFILE_SCOPE("PlayerAttackState::Execute");
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    // 攻撃中も入力をBlackboardへ流し、コンボ分岐やキャンセル判定に使える状態を保つ。
    attackElapsedTime += (std::max)(elapsedTime_, 0.0f);
    ApplyAttackAssist(elapsedTime_);
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

// 攻撃を抜ける時に先行入力受付を閉じ、設定に応じて補正Targetを破棄する。
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
        if (action == PLAYER_ACTION::WEAK_ATTACK)
        {
            // Animation更新後のキャンセルでも、当フレームの武器判定を残さない。
            if (const auto animation_ = GetComponent<AnimationControllerComponent>())
            {
                const auto state_ = animation_->GetCurrentState();
                if (state_ && state_->sampleWeaponHitBoxes)
                    for (auto& event_ : const_cast<AnimState*>(state_)->events)
                        if (event_.isFired && event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
                            AttackTemplateRuntime::End(GetOwner(), event_);
            }
            if (const auto warp_ = GetComponent<MotionWarpComponent>()) warp_->Stop(weakMotionWarpPresetName);
            weakMotionWarpStarted = false;
            weakMotionWarpPresetName.clear();
            attackAssistTarget.reset();
        }
        StopCounterMotionWarp();
    }
}

// 攻撃開始時に使う補正対象を、ロックオン、入力方向、カメラ中央、距離の優先度で選ぶ。
std::shared_ptr<GameObject> PlayerAttackState::FindAssistTarget()
{
    BS_PROFILE_SCOPE("PlayerAttackState::FindAssistTarget");
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
        if (action == PLAYER_ACTION::WEAK_ATTACK && !IsWeakComboTargetValid(target_)) return false;

        const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();
        const float differenceX_ = targetPosition_.x - ownerPosition_.x;
        const float differenceZ_ = targetPosition_.z - ownerPosition_.z;
        const float horizontalDistanceSquared_ = differenceX_ * differenceX_ + differenceZ_ * differenceZ_;
        const float assistDistanceSquared_ =
            CalculateAttackAssistDistanceSquared(ownerPosition_, targetPosition_);
        return
            assistDistanceSquared_ <= maximumDistanceSquared_ &&
            horizontalDistanceSquared_ > MINIMUM_HORIZONTAL_LENGTH_SQUARED;
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
        : (action == PLAYER_ACTION::WEAK_ATTACK ? ownerForward_
            : (settings_.preferCameraForwardDirection && hasCameraForward_ ? cameraForward_ : ownerForward_));

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
        const float horizontalDistanceSquared_ = differenceX_ * differenceX_ + differenceZ_ * differenceZ_;
        const float assistDistanceSquared_ =
            CalculateAttackAssistDistanceSquared(ownerPosition_, candidatePosition_);

        DirectX::XMFLOAT3 targetDirection_{ differenceX_, 0.0f, differenceZ_ };
        if (!NormalizeHorizontal(targetDirection_)) continue;

        const float primaryDot_ = DotHorizontal(targetDirection_, primaryDirection_);
        if (primaryDot_ < targetThreshold_) continue;

        if (assistDistanceSquared_ < nearestDistanceSquared_)
        {
            nearestDistanceSquared_ = assistDistanceSquared_;
            nearestTarget_ = candidate_;
        }

        const float cameraDot_ = hasCameraForward_
            ? DotHorizontal(targetDirection_, cameraForward_)
            : primaryDot_;
        const float ownerDot_ = DotHorizontal(targetDirection_, ownerForward_);
        const float distance_ = std::sqrt(assistDistanceSquared_);
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

// コンボ中のTarget維持や入力方向優先を考慮して、今回の攻撃補正基準を確定する。
void PlayerAttackState::ResolveAttackAssistTarget()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!owner_ || !owner_->transform || !player_) return;

    // コンボ中の補正先継続や入力方向優先をここで決め、攻撃中に使う基準方向を固定する。
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
    const bool isWeakComboContinuation_ = action == PLAYER_ACTION::WEAK_ATTACK && player_->GetComboIndex() > 0;
    const auto lockOn_ = ResolveCameraLockOnComponent();
    // 明示的なロックオン変更だけは次段の入口で優先し、実行途中には切り替えない。
    if (action == PLAYER_ACTION::WEAK_ATTACK && lockOn_ && lockOn_->IsLockOnEnabled() &&
        IsWeakComboTargetValid(lockOn_->GetLockOnTarget())) target_ = lockOn_->GetLockOnTarget();
    if (!target_ && settings_.preserveTargetAcrossCombo &&
        (action != PLAYER_ACTION::WEAK_ATTACK || isWeakComboContinuation_))
    {
        const std::shared_ptr<GameObject> preservedTarget_ =
            player_->GetAttackAssistTarget();
        if (preservedTarget_ &&
            preservedTarget_->transform &&
            preservedTarget_->layer == LayerType::Enemy &&
            (action != PLAYER_ACTION::WEAK_ATTACK || IsWeakComboTargetValid(preservedTarget_)))
        {
            target_ = preservedTarget_;
        }
    }

    if (settings_.lockTargetOnAttackEnter)
    {
        if (!target_ && (!isWeakComboContinuation_ || !settings_.preserveTargetAcrossCombo || moveInputLength_ >= 0.65f))
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

// 攻撃中のTargetが有効なら、向き補正と位置補正を順に適用する。
void PlayerAttackState::ApplyAttackAssist(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform) return;

    // 攻撃中の見た目と手触りを補助するため、向き補正と位置補正を分けて適用する。
    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    if (!settings_.enabled || settings_.searchDistance <= 0.0f)
    {
        if (action == PLAYER_ACTION::WEAK_ATTACK)
            if (const auto warp_ = GetComponent<MotionWarpComponent>()) warp_->Stop(weakMotionWarpPresetName);
        return;
    }

    if (!attackAssistTargetResolved || (action != PLAYER_ACTION::WEAK_ATTACK && settings_.allowRetargetDuringAttack))
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

    if (action == PLAYER_ACTION::WEAK_ATTACK)
    {
        // 弱コンボは位置・向きをWarpへ一本化し、従来の速度加算を重ねない。
        if (!IsWeakComboTargetValid(target_))
        {
            if (const auto warp_ = GetComponent<MotionWarpComponent>()) warp_->Stop(weakMotionWarpPresetName);
            player_->ClearAttackAssistTarget();
            attackAssistTarget.reset();
        }
        return;
    }
    ApplyAttackFacingAssist(target_, elapsedTime_);
    ApplyAttackPositionAssist(target_, elapsedTime_);
}


// Animation時間に応じた重みで、Playerの向きをTarget方向へ少しずつ寄せる。
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

    // Animation時間に応じて補正量を変え、攻撃終盤で不自然に回り続けないようにする。
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

    // 往復する補正も絶対角度で累積し、長い攻撃で追従が無制限に続くことを防ぐ。
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

// Targetとの間合いが遠い場合だけ、停止距離を残して軽く引き寄せる。
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
    // 見た目の停止距離よりColliderの合計半径が大きい場合は、判定同士の間隔を優先する。
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

    // 近づきすぎない停止距離を残しつつ、届きにくい攻撃だけを軽く吸い付かせる。
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

// AnimationControllerの再生時間を優先し、取得できない場合はState経過時間を代用する。
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

// Scene内からCameraLockOnComponentを探し、攻撃Target選択の優先情報として使う。
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

// 物理的な壁だけを接近経路へ含め、所有者と狙っている敵の判定は除外する。
namespace
{
    // 同じCollisionOwnerと対象の物理形状を除外する接近用フィルター。
    class AttackPathFilter : public physx::PxQueryFilterCallback
    {
    public:
        AttackPathFilter(uint32_t ownerId_, uint32_t targetId_)
            : ownerId(ownerId_), targetId(targetId_) {}
        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape* shape_,
            const physx::PxRigidActor*, physx::PxHitFlags&) override
        {
            if (!shape_) return physx::PxQueryHitType::eNONE;
            constexpr uint32_t COLLISION_OWNER_MASK = 0x7FFFFFFFu;
            const uint32_t shapeOwner_ = shape_->getSimulationFilterData().word3 & COLLISION_OWNER_MASK;
            const auto role_ = static_cast<ColliderComponent::CollisionRole>(shape_->getSimulationFilterData().word2);
            if ((ownerId != 0 && shapeOwner_ == ownerId) || (targetId != 0 && shapeOwner_ == targetId) ||
                shape_->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE) ||
                (role_ != ColliderComponent::CollisionRole::PHYSICAL && role_ != ColliderComponent::CollisionRole::CHARACTER))
                return physx::PxQueryHitType::eNONE;
            return physx::PxQueryHitType::eBLOCK;
        }
        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit&,
            const physx::PxShape*, const physx::PxRigidActor*) override { return physx::PxQueryHitType::eBLOCK; }
    private:
        uint32_t ownerId = 0;
        uint32_t targetId = 0;
    };

    // キャラクターの胴体の高さでSphere Sweepし、壁越しの選択と補正を止める。
    bool IsAttackPathClear(const std::shared_ptr<GameObject>& owner_, const std::shared_ptr<GameObject>& target_,
        const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_)
    {
        if (!owner_ || !owner_->transform || !target_ || !target_->transform) return false;
        const auto scene_ = PhysXSystem::GetScene();
        const auto capsule_ = owner_ ? owner_->GetComponent<CapsuleCollider>() : nullptr;
        if (!scene_ || !capsule_) return false;
        const auto targetCapsule_ = target_ ? target_->GetComponent<CapsuleCollider>() : nullptr;
        AttackPathFilter filter_(capsule_->GetCollisionOwnerId(), targetCapsule_ ? targetCapsule_->GetCollisionOwnerId() : 0u);
        auto data_ = physx::PxQueryFilterData(physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER);
        const auto shape_ = capsule_->GetCapsuleData();
        DirectX::XMFLOAT3 center_{};
        DirectX::XMStoreFloat3(&center_, DirectX::XMVectorScale(DirectX::XMVectorAdd(shape_.p0, shape_.p1), 0.5f));
        const auto ownerPosition_ = owner_->transform->GetWorldPosition();
        const physx::PxVec3 origin_(center_.x + start_.x - ownerPosition_.x, center_.y, center_.z + start_.z - ownerPosition_.z);
        const physx::PxVec3 direction_(end_.x - start_.x, 0.0f, end_.z - start_.z);
        const float length_ = direction_.magnitude();
        if (length_ <= 0.0001f) return true;
        physx::PxSweepBuffer hit_;
        return !scene_->sweep(physx::PxSphereGeometry((std::max)(shape_.radius * 0.9f, 0.05f)),
            physx::PxTransform(origin_), direction_ / length_, length_, hit_, physx::PxHitFlag::eDEFAULT, data_, &filter_);
    }
}

// 判定開始から安全幅を引いた窓を使い、段ごとの保存済み設定を優先する。
void PlayerAttackState::PrepareWeakComboMotionWarp()
{
    BS_PROFILE_SCOPE("PlayerAttackState::PrepareWeakComboMotionWarp");
    const auto owner_ = GetOwner();
    const auto player_ = GetPlayerController();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!player_ || !animation_ || !model_ || !owner_->transform) return;
    const int stage_ = std::clamp(player_->GetComboIndex(), 0, 3);
    weakMotionWarpPresetName = "WeakAttackWarp" + std::to_string(stage_ + 1);
    const std::string stateName_ = "SS_Attack" + std::to_string(stage_ + 1);
    const auto iterator_ = animation_->GetStates().find(stateName_);
    if (iterator_ == animation_->GetStates().end() || !iterator_->second) return;
    const auto& state_ = *iterator_->second;
    if (state_.animationIndex < 0) return;
    if (!state_.speedCurveName.empty())
    {
        const auto curve_ = owner_->GetOrAddComponent<CurveComponent>();
        if (!curve_->HasTrack(state_.speedCurveName)) curve_->LoadTrackAsset("Data/Curve/" + state_.speedCurveName + ".json");
    }
    const float duration_ = model_->GetAnimationDuration(state_.animationIndex);
    float hitStart_ = duration_;
    for (const auto& event_ : state_.events)
        if (event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
            hitStart_ = (std::min)(hitStart_, event_.startTime);
    if (duration_ <= 0.0f || hitStart_ <= 0.025f) return;
    weakWarpEndNormalizedTime = (hitStart_ - 0.025f) / duration_;
    const auto warp_ = owner_->GetOrAddComponent<MotionWarpComponent>();
    MotionWarpPreset preset_;
    preset_.presetName = weakMotionWarpPresetName;
    preset_.positionMode = MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
    preset_.targetDistance = player_->GetAttackAssistSettings().targetStopDistance;
    preset_.maximumTotalPositionCorrection = stage_ == 0 ? 2.5f : 1.25f;
    preset_.maximumPositionCorrectionPerSecond = stage_ == 0 ? 24.0f : 20.0f;
    preset_.maximumPositionCorrectionPerFrame = 2.5f;
    preset_.discardUnappliedCorrection = true;
    preset_.maximumRotationDegrees = stage_ == 0 ? 55.0f : 45.0f;
    preset_.maximumRotationDegreesPerSecond = 540.0f;
    preset_.lockTargetOnStart = false;
    preset_.endNormalizedTime = weakWarpEndNormalizedTime;
    preset_.lockTargetNormalizedTime = (std::max)(0.0f, weakWarpEndNormalizedTime - 0.02f);
    preset_.rotationFullStrengthEndNormalizedTime = preset_.lockTargetNormalizedTime * 0.75f;
    preset_.rotationEndNormalizedTime = preset_.lockTargetNormalizedTime;
    preset_.maximumTargetTravelDistance = 0.6f;
    preset_.maximumTargetTravelAngleDegrees = 15.0f;
    warp_->AddPresetIfMissing(preset_);
    const auto configured_ = warp_->GetPreset(weakMotionWarpPresetName);
    if (!configured_) return;
    weakWarpEndNormalizedTime = (std::min)(weakWarpEndNormalizedTime, configured_->endNormalizedTime);
    DirectX::XMFLOAT3 localMotion_{};
    float playbackDuration_ = 0.0f;
    animation_->EstimateRootMotion(stateName_, configured_->startNormalizedTime, weakWarpEndNormalizedTime, localMotion_, playbackDuration_);
    const auto rotation_ = owner_->transform->localRotation;
    const auto quaternion_ = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
        DirectX::XMConvertToRadians(rotation_.y), DirectX::XMConvertToRadians(rotation_.x), DirectX::XMConvertToRadians(rotation_.z));
    const auto planned_ = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3(localMotion_), quaternion_);
    weakPlannedRootMotion = { planned_.x, 0.0f, planned_.z };
}

// 届く見込みのある生存中の敵だけを残し、背後への自動吸着や壁越しの選択を防ぐ。
bool PlayerAttackState::IsWeakComboTargetValid(const std::shared_ptr<GameObject>& target_) const
{
    const auto owner_ = GetOwner();
    const auto player_ = GetPlayerController();
    if (!owner_ || !owner_->transform || !player_ || !target_ || !target_->transform ||
        target_->layer != LayerType::Enemy || target_->IsPendingDestroy() || !target_->IsCombatReady()) return false;
    if (const auto health_ = target_->GetComponent<HealthComponent>()) if (health_->IsDead()) return false;
    if (!HasActiveAttackReceiver(target_)) return false;
    const auto warp_ = owner_->GetComponent<MotionWarpComponent>();
    const auto preset_ = warp_ ? warp_->GetPreset(weakMotionWarpPresetName) : nullptr;
    if (!preset_) return false;
    const auto position_ = owner_->transform->GetWorldPosition();
    const auto targetPosition_ = target_->transform->GetWorldPosition();
    DirectX::XMFLOAT3 direction_{ targetPosition_.x - position_.x, 0.0f, targetPosition_.z - position_.z };
    const float distance_ = std::sqrt(direction_.x * direction_.x + direction_.z * direction_.z);
    if (std::abs(targetPosition_.y - position_.y) > 3.0f ||
        distance_ > (std::min)(preset_->maximumWarpTargetDistance, player_->GetAttackAssistSettings().searchDistance)) return false;
    if (distance_ <= 0.0001f) return true;
    NormalizeHorizontal(direction_);
    const float stopDistance_ = (std::max)(preset_->targetDistance, GetWorldCapsuleRadius(owner_) + GetWorldCapsuleRadius(target_) + 0.10f);
    const float plannedApproach_ = (std::max)(0.0f, weakPlannedRootMotion.x * direction_.x + weakPlannedRootMotion.z * direction_.z);
    if (distance_ > stopDistance_ + plannedApproach_ + preset_->maximumTotalPositionCorrection) return false;
    // 新規選択時だけ向きの到達可能角度を調べ、同じ段の旋回による対象の振動を防ぐ。
    if (!weakMotionWarpStarted)
    {
        auto forward_ = player_->GetMoveInput();
        if (!NormalizeHorizontal(forward_))
        {
            const auto front_ = owner_->transform->GetFront();
            forward_ = { front_.x, 0.0f, front_.z };
            NormalizeHorizontal(forward_);
        }
        if (DotHorizontal(direction_, forward_) < std::cos(preset_->maximumRotationDegrees * DEGREES_TO_RADIANS)) return false;
    }
    return IsAttackPathClear(owner_, target_, position_, targetPosition_);
}

// 元のRootMotionを予定量として渡し、不足分だけを同じ移動経路へ加える。
void PlayerAttackState::StartWeakComboMotionWarp()
{
    BS_PROFILE_SCOPE("PlayerAttackState::StartWeakComboMotionWarp");
    const auto owner_ = GetOwner();
    const auto player_ = GetPlayerController();
    const auto target_ = attackAssistTarget.lock();
    const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
    const auto preset_ = warp_ ? warp_->GetPreset(weakMotionWarpPresetName) : nullptr;
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto activeState_ = animation_ ? animation_->GetCurrentState() : nullptr;
    if (!player_ || !player_->GetAttackAssistSettings().enabled || !preset_ || !activeState_ ||
        activeState_->name != "SS_Attack" + std::to_string(player_->GetComboIndex() + 1) || !IsWeakComboTargetValid(target_)) return;
    weakStopDistance = (std::max)(preset_->targetDistance, GetWorldCapsuleRadius(owner_) + GetWorldCapsuleRadius(target_) + 0.10f);
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    const std::weak_ptr<GameObject> weakTarget_ = target_;
    weakMotionWarpStarted = warp_->PlayForTarget(weakMotionWarpPresetName, target_, weakPlannedRootMotion, weakStopDistance,
        preset_->startNormalizedTime, weakWarpEndNormalizedTime, nullptr,
        [weakOwner_, weakTarget_](const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_)
        {
            const auto owner_ = weakOwner_.lock();
            const auto target_ = weakTarget_.lock();
            if (!owner_ || !target_ || target_->IsPendingDestroy() || !target_->IsCombatReady()) return false;
            if (const auto health_ = target_->GetComponent<HealthComponent>()) if (health_->IsDead()) return false;
            if (!HasActiveAttackReceiver(target_)) return false;
            return IsAttackPathClear(owner_, target_, start_, end_);
        });
}

// 反撃だけに使う広い補正Presetを開始し、このStateが停止すべきWarpとして記録する。
void PlayerAttackState::StartCounterMotionWarp()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return;

    // 反撃は成功後のご褒美攻撃なので、通常攻撃より広い補正Presetを明示して再生する。
    const std::shared_ptr<MotionWarpComponent> motionWarp_ =
        owner_->GetOrAddComponent<MotionWarpComponent>();
    if (!motionWarp_) return;

    motionWarp_->Play("CounterAttackWarp");
    counterMotionWarpActive = true;
}

// このStateが開始した反撃用Presetだけを停止し、終了・中断後の補正残りを防ぐ。
void PlayerAttackState::StopCounterMotionWarp()
{
    if (!counterMotionWarpActive) return; 

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    const std::shared_ptr<MotionWarpComponent> motionWarp_ =
        owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
    if (motionWarp_)
    {
        motionWarp_->Stop("CounterAttackWarp");
    }

    counterMotionWarpActive = false;
}
