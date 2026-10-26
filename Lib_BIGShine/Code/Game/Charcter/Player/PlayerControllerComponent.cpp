#include "PrecompiledHeader.h"

#include "PlayerControllerComponent.h"

#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/System/TimeManager.h"

#include <algorithm>
#include <cmath>
#include <imgui_stdlib.h>

#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Combat/HitReactionComponent.h"
#include "Engine/GamePlay/Combat/JustDodgeReactionComponent.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Physics/CharacterCollider.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Physics/JustDodgeCollider.h"

#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
#include "Game/GameMode/GameModeManager.h"
#include "PlayerActionState.h"
#include "PlayerInputResolver.h"
#include "PlayerSkillState.h"
#include "PlayerAttackState.h"
#include "PlayerDodgeState.h"
#include "PlayerHitState.h"
#include "PlayerDeathState.h"
#include "PlayerIdleState.h"
#include "PlayerJumpState.h"
#include "PlayerMoveState.h"

namespace
{
    inline constexpr int WEAK_ATTACK_COMBO_COUNT = 4;

    // KeyConfigへ登録するPlayer用Action名。JSONやEditor設定と接続するため文字列は変更しない。
    namespace PlayerInputAction
    {
        inline constexpr const char* MOVE_X = "Player.MoveX";
        inline constexpr const char* MOVE_Z = "Player.MoveZ";
        inline constexpr const char* WEAK_ATTACK = "Player.WeakAttack";
        inline constexpr const char* STRONG_ATTACK = "Player.StrongAttack";
        inline constexpr const char* JUMP = "Player.Jump";
        inline constexpr const char* LOCK_ON = "Player.LockOn";
        inline constexpr const char* GUARD = "Player.Guard";
        inline constexpr const char* JUST_GUARD = "Player.JustGuard";
        inline constexpr const char* DODGE = "Player.Dodge";
        inline constexpr const char* SKILL_A = "Player.SkillA";
        inline constexpr const char* SKILL_B = "Player.SkillB";
        inline constexpr const char* SKILL_C = "Player.SkillC";
        inline constexpr const char* SKILL_D = "Player.SkillD";
    }

    DirectX::XMFLOAT3 NormalizeHorizontalDirection(
        const DirectX::XMFLOAT3& direction_,
        const DirectX::XMFLOAT3& fallback_)
    {
        // Player操作では高さ方向を捨て、XZ平面だけの向きとして扱う。
        DirectX::XMFLOAT3 result_{ direction_.x, 0.0f, direction_.z };
        const float lengthSquared_ = result_.x * result_.x + result_.z * result_.z;
        if (lengthSquared_ <= 0.0001f)
        {
            return fallback_;
        }

        const float inverseLength_ = 1.0f / std::sqrt(lengthSquared_);
        result_.x *= inverseLength_;
        result_.z *= inverseLength_;
        return result_;
    }

    DirectX::XMFLOAT3 GetCameraForwardDirection(const std::shared_ptr<GameObject>& owner_)
    {
        // ActiveCameraのView行列から水平前方向を復元し、カメラ基準入力の基準軸にする。
        if (!owner_ || !owner_->manager) return { 0.0f, 0.0f, 1.0f };

        auto* cameraManager_ = owner_->GetManager().GetCameraManager();
        if (!cameraManager_) return { 0.0f, 0.0f, 1.0f };

        auto camera_ = cameraManager_->GetActiveCamera();
        if (!camera_) return { 0.0f, 0.0f, 1.0f };

        const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&camera_->GetView());
        DirectX::XMVECTOR determinant_;
        const DirectX::XMMATRIX inverseView_ = DirectX::XMMatrixInverse(&determinant_, view_);
        const DirectX::XMVECTOR forward_ = DirectX::XMVector3TransformNormal(
            DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            inverseView_);

        DirectX::XMFLOAT3 direction_;
        DirectX::XMStoreFloat3(&direction_, forward_);
        return NormalizeHorizontalDirection(direction_, { 0.0f, 0.0f, 1.0f });
    }

    DirectX::XMFLOAT3 GetCameraRelativeMoveDirection(
        const std::shared_ptr<GameObject>& owner_,
        const DirectX::XMFLOAT3& input_)
    {
        // 入力AxisをCameraのForward/Rightへ合成し、画面基準の移動方向へ変換する。
        const float inputLengthSquared_ = input_.x * input_.x + input_.z * input_.z;
        if (inputLengthSquared_ <= 0.0001f)
        {
            return { 0.0f, 0.0f, 0.0f };
        }

        if (!owner_ || !owner_->manager)
        {
            return NormalizeHorizontalDirection(input_, { 0.0f, 0.0f, 1.0f });
        }

        auto* cameraManager_ = owner_->GetManager().GetCameraManager();
        auto camera_ = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
        if (!camera_)
        {
            return NormalizeHorizontalDirection(input_, { 0.0f, 0.0f, 1.0f });
        }

        const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&camera_->GetView());
        DirectX::XMVECTOR determinant_;
        const DirectX::XMMATRIX inverseView_ = DirectX::XMMatrixInverse(&determinant_, view_);

        DirectX::XMFLOAT3 forward_;
        DirectX::XMFLOAT3 right_;
        DirectX::XMStoreFloat3(
            &forward_,
            DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), inverseView_));
        DirectX::XMStoreFloat3(
            &right_,
            DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), inverseView_));

        forward_ = NormalizeHorizontalDirection(forward_, { 0.0f, 0.0f, 1.0f });
        right_ = NormalizeHorizontalDirection(right_, { 1.0f, 0.0f, 0.0f });

        DirectX::XMFLOAT3 direction_
        {
            right_.x * input_.x + forward_.x * input_.z,
            0.0f,
            right_.z * input_.x + forward_.z * input_.z
        };
        return NormalizeHorizontalDirection(direction_, { 0.0f, 0.0f, 1.0f });
    }

}

void PlayerControllerComponent::EnsurePlayerInputBindings()
{
    // プロジェクト側でまだ入力設定が無い場合だけ、最低限のPlayer操作を登録する。
    KeyConfig& keyConfig_ = KeyConfig::Instance();
    // 物理ボタンへの依存は初期バインドだけに限定し、判定はInput Action名で行う。
    if (keyConfig_.GetActionMap().count("Player.SkillModifier") == 0)
    {
        keyConfig_.SetBinds("Player.SkillModifier",
            { { { DeviceType::Gamepad, static_cast<int>(PadCode::L1) } } });
    }
    // 4枠目は既存設定を優先し、入力がない場合だけYキーで回復スキルを使えるようにする。
    if (keyConfig_.GetActionMap().count(PlayerInputAction::SKILL_D) == 0)
    {
        keyConfig_.SetBinds(PlayerInputAction::SKILL_D,
            { { { DeviceType::KBM, KeyInput::K_Y } } });
    }
    if (keyConfig_.GetActionMap().count(PlayerInputAction::JUST_GUARD) == 0)
    {
        // 既存の割り当てを維持し、未設定の場合だけCキーをパリィに割り当てる。
        keyConfig_.SetBinds(PlayerInputAction::JUST_GUARD,
            { { { DeviceType::KBM, KeyInput::K_C } } });
    }
    if (keyConfig_.GetActionMap().count(PlayerInputAction::JUMP) == 0)
    {
        keyConfig_.SetBinds(
            PlayerInputAction::JUMP,
            {
                { { DeviceType::KBM, KeyInput::K_Space } },
                { { DeviceType::Gamepad, static_cast<int>(PadCode::A) } }
            });
    }
    if (keyConfig_.GetActionMap().count(PlayerInputAction::LOCK_ON) == 0)
    {
        keyConfig_.SetBinds(
            PlayerInputAction::LOCK_ON,
            {
                { { DeviceType::KBM, KeyInput::K_Q } },
                { { DeviceType::Gamepad, static_cast<int>(PadCode::R3) } }
            });
    }
}

void PlayerControllerComponent::Start()
{
    // Scene上のPlayer Objectが動作に必要なComponentとAction登録を持つ状態へ整える。
    auto owner_ = GetGameObject();
    if (!owner_) return;

    // Playerに必要な基本Componentをここでそろえ、Scene配置側の付け忘れを吸収する。
    EnsurePlayerInputBindings();
    if (!LoadPlayerSkillDefinitions(skillDefinitionsPath, skillInputs, skillDefinitions))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "Player skill definitions could not be loaded: " + skillDefinitionsPath);
    }

    ApplyLegacySkillLoadout();
    ResetSkillCooldowns();
    hudObservationTime = 0.0f;
    skillHudAssetCache = {};
    owner_->SetLayer(LayerType::Player);
    collider = owner_->GetOrAddComponent<CharacterCollider>();
    movement = owner_->GetOrAddComponent<CharacterMovementComponent>();
    hurtBox = owner_->GetOrAddComponent<HurtBox>();
    justDodgeCollider = owner_->GetOrAddComponent<JustDodgeCollider>();
    owner_->GetOrAddComponent<JustDodgeReactionComponent>();

    if (collider)
    {
        collider->mass = (std::max)(collider->mass, 1.0f);
        collider->RecreatePhysXActor();
    }

    blackboard = owner_->GetOrAddComponent<BlackboardComponent>();
    InitializeBlackboardValues();

    owner_->GetOrAddComponent<HealthComponent>();
    RegisterPlayerAnimationActions();

    const std::shared_ptr<HitReactionComponent> hitReaction_ =
        owner_->GetOrAddComponent<HitReactionComponent>();
    if (hitReaction_)
    {
        hitReaction_->SetArmor(GetCurrentArmor());
        hitReaction_->SetReactionImpactThresholds(
            hitMediumImpactThreshold,
            hitHeavyImpactThreshold,
            airHitKnockdownImpactThreshold);
        hitReaction_->SetDamageActionName("Health_Hit");
        hitReaction_->SetReactionActionName("Player_Hit");
    }

    const std::shared_ptr<ActionSequence> hitSequence_ =
        owner_->GetOrAddComponent<ActionSequence>();

    if (hitSequence_)
    {
        // 被弾時はHealth処理とPlayer状態遷移を同じSequenceから即時実行する。
        hitSequence_->SetRegisterGlobal(false);
        ActionReference healthAction_{};
        healthAction_.category = "Health";
        healthAction_.actionName = "Health_Hit";
        ActionReference playerAction_{};
        playerAction_.category = "Player";
        playerAction_.actionName = "Player_Hit";
        hitSequence_->EnsureImmediateSequence(
            "Hit_Player",
            { healthAction_, playerAction_ });
    }

    stateMachine.Initialize(*this);
    // 入力種別ごとにStateを登録し、巨大なUpdate分岐ではなくState単位で処理を分ける。
    stateMachine.AddState<PlayerIdleState>(PLAYER_ACTION::IDLE);
    stateMachine.AddState<PlayerMoveState>(PLAYER_ACTION::MOVE);
    stateMachine.AddState<PlayerDodgeState>(PLAYER_ACTION::DODGE);
    stateMachine.AddState<PlayerDodgeState>(PLAYER_ACTION::AIR_DODGE, PLAYER_ACTION::AIR_DODGE);
    stateMachine.AddState<PlayerJumpState>(PLAYER_ACTION::JUMP);
    stateMachine.AddState<PlayerHitState>(PLAYER_ACTION::HIT);
    stateMachine.AddState<PlayerAttackState>(
        PLAYER_ACTION::WEAK_ATTACK,
        PLAYER_ACTION::WEAK_ATTACK);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::STRONG_ATTACK);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::STRONG_CHARGE_ATTACK, PLAYER_ACTION::STRONG_CHARGE_ATTACK);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::AIR_WEAK_ATTACK, PLAYER_ACTION::AIR_WEAK_ATTACK);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::AIR_STRONG_ATTACK, PLAYER_ACTION::AIR_STRONG_ATTACK);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK, PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::GUARD, PLAYER_ACTION::GUARD);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::JUST_GUARD);
    stateMachine.AddState<PlayerSkillState>(PLAYER_ACTION::SKILL_A, PLAYER_ACTION::SKILL_A);
    stateMachine.AddState<PlayerSkillState>(PLAYER_ACTION::SKILL_B, PLAYER_ACTION::SKILL_B);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::COUNTER_ATTACK, PLAYER_ACTION::COUNTER_ATTACK);
    stateMachine.AddState<PlayerSkillState>(PLAYER_ACTION::SKILL_C, PLAYER_ACTION::SKILL_C);
    // 回復も共通SkillStateへ接続し、既存のPhase実行と終了処理を使用する。
    stateMachine.AddState<PlayerSkillState>(PLAYER_ACTION::SKILL_D, PLAYER_ACTION::SKILL_D);
    stateMachine.AddState<PlayerDeathState>(PLAYER_ACTION::DEAD);
    stateMachine.AddState<PlayerReviveState>(PLAYER_ACTION::REVIVING);
    EnsureDefaultCancelTransitions();

    stateMachine.ChangeState(PLAYER_ACTION::IDLE);
}

void PlayerControllerComponent::Update(float dt_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::Update");
    lastUpdateDeltaTime = (std::max)(dt_, 0.0f);
    hudObservationTime += lastUpdateDeltaTime;
    // 死亡・復帰中はPlayerステートだけを進め、操作ロック中もグラフの終了を確認する。
    if (GetCurrentAction() == PLAYER_ACTION::DEAD || GetCurrentAction() == PLAYER_ACTION::REVIVING)
    {
        stateMachine.Update(dt_);
        return;
    }
    // 拘束・復帰の専用再生を通常Stateより優先し、この間は通常入力と移動更新へ進まない。
    if ((enemyExecutionCaptured || enemyExecutionRecovering) && IsPlayerControlLockedByGameMode()) CancelEnemyExecution(false);
    if (UpdateEnemyExecution(dt_))
    {
        UpdateSkillCooldowns(dt_);
        SendGroundedToBlackboard();
        return;
    }
    if (IsPlayerControlLockedByGameMode())
    {
        StopNormalGuard();
        if (GetCurrentAction() == PLAYER_ACTION::GUARD) stateMachine.ChangeState(PLAYER_ACTION::IDLE);
        // 演出中に直前Frameの移動予約や入力が残らないよう、入力由来の値だけを消す。
        pendingHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
        hasPendingHorizontalVelocity = true;
        hasPendingFacingDirection = false;
        SendMoveInputToBlackboard({ 0.0f, 0.0f, 0.0f });
        SendGuardInputToBlackboard(false);
        ResetStrongAttackInput();
        // 演出Stateを通常のIdle/Move更新で上書きせず、予約済み移動だけ停止する。
        ApplyPendingMovement();
        SendGroundedToBlackboard();
        return;
    }

    const bool guardHeld_ = KeyConfig::Instance().ActionPressed(PlayerInputAction::GUARD);
    if (!guardHeld_ && !guardBroken) guardRearmRequired = false;
    if (GetCurrentAction() == PLAYER_ACTION::GUARD && !guardBroken && !guardHeld_)
    {
        // 判定を先に閉じ、解除アニメーションやActionEndを待たずに通常Stateへ戻す。
        StopNormalGuard();
        SendGuardInputToBlackboard(false);
        const auto input_ = GetMoveInput();
        stateMachine.ChangeState(input_.x != 0.0f || input_.z != 0.0f ? PLAYER_ACTION::MOVE : PLAYER_ACTION::IDLE);
    }
    UpdateGuardReaction(dt_);

    // State更新後に、キャンセル受付・空中状態・Blackboard・移動反映をまとめて後処理する。
    UpdateSkillCooldowns(dt_);
    stateMachine.Update(dt_);
    TryDodgeCancelInput();
    UpdateAirborneActionContext();
    UpdateDodgeRestrictions(dt_);
    UpdateCounterTags(dt_);
    SendGroundedToBlackboard();
    UpdateLocomotionAnimationPhase(dt_);
    UpdateTimelineActionSafety(dt_);
    ApplyPendingMovement();
}

bool PlayerControllerComponent::IsGrounded() const
{
    return collider && collider->isGrounded;
}

bool PlayerControllerComponent::IsJumpInputPressed() const
{
    if (IsPlayerControlLockedByGameMode()) return false;
    // 回復に使う複合入力は二段ジャンプ側でも消費済みとして扱う。
    const auto& keyConfig_ = KeyConfig::Instance();
    return !PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::JUMP) &&
        keyConfig_.ActionPressedIn(PlayerInputAction::JUMP);
}

bool PlayerControllerComponent::IsCurrentAnimationState(const std::string& stateName_)
{
    // Timeline通知の補助として、現在AnimationState名を直接確認する。
    const auto owner_ = GetGameObject();
    if (!owner_) return false;

    const auto animationController_ = owner_->GetComponent<AnimationControllerComponent>();
    if (!animationController_) return false;

    const AnimState* currentState_ = animationController_->GetCurrentState();
    return currentState_ && currentState_->name == stateName_;
}

bool PlayerControllerComponent::IsAnimationStateForAction(
    PLAYER_ACTION action_,
    const std::string& stateName_) const
{
    // 空中Actionなど、同じActionでもAnimation名が限定されるものだけ個別に検証する。
    switch (action_)
    {
    case PLAYER_ACTION::AIR_WEAK_ATTACK:
        return
            stateName_ == "Combo_Attack_Air_01_Seq" ||
            stateName_ == "Combo_Attack_Air_02_Seq" ||
            stateName_ == "Combo_Attack_Air_03_Seq" ||
            stateName_ == "Combo_Attack_Air_04_Seq";
    case PLAYER_ACTION::AIR_STRONG_ATTACK:
        return stateName_ == "Dash_Air_Attack_Seq";
    case PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK:
        return stateName_ == "Attack_Air_To_Floor_Start_01_Seq_0";
    case PLAYER_ACTION::AIR_DODGE:
        return stateName_ == "AirDodge_Blend";
    case PLAYER_ACTION::COUNTER_ATTACK:
        return stateName_ == "Run_Attack_01_Seq";
    default:
        return true;
    }
}

void PlayerControllerComponent::UpdateTimelineActionSafety(float deltaTime_)
{
    // 共通スキルは自身のPhase終了条件を持つため、通常行動の保険時間では終了させない。
    const auto* skillState_ = stateMachine.GetCurrentStateAs<PlayerSkillState>();
    if (skillState_ && skillState_->IsActive()) return;
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (!IsPlayerActionTimelineDriven(currentAction_) || actionRuntime.actionEnded)
    {
        timelineActionSafetyTimer = 0.0f;
        return;
    }

    // Timeline終了イベントが来ない場合でも、アニメーション終了やfallback時間で行動を閉じる保険。
    timelineActionSafetyTimer += (std::max)(deltaTime_, 0.0f);

    const auto owner_ = GetGameObject();
    const auto animationController_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (!animationController_)
    {
        if (timelineActionFallbackDuration > 0.0f &&
            timelineActionSafetyTimer >= timelineActionFallbackDuration)
        {
            NotifyActionEnd();
        }
        return;
    }

    const AnimState* currentState_ = animationController_->GetCurrentState();
    if (!currentState_)
    {
        if (timelineActionFallbackDuration > 0.0f &&
            timelineActionSafetyTimer >= timelineActionFallbackDuration)
        {
            NotifyActionEnd();
        }
        return;
    }

    if (currentAction_ == PLAYER_ACTION::HIT &&
        currentState_->name == "SS_HitAir_Large_Loop")
    {
        return;
    }

    if (!IsAnimationStateForAction(currentAction_, currentState_->name))
    {
        if (timelineActionFallbackDuration > 0.0f &&
            timelineActionSafetyTimer >= timelineActionFallbackDuration)
        {
            NotifyActionEnd();
        }
        return;
    }

    if (currentState_->isLoop)
    {
        if (timelineActionFallbackDuration > 0.0f &&
            timelineActionSafetyTimer >= timelineActionFallbackDuration)
        {
            NotifyActionEnd();
        }
        return;
    }

    const float sampleDuration_ = animationController_->GetCurrentSampleDuration();
    if (sampleDuration_ > 0.0001f &&
        animationController_->GetAnimatorTime() >= sampleDuration_)
    {
        NotifyActionEnd();
    }
}

void PlayerControllerComponent::BeginAirborneActionContext()
{
    // 空中Action中の接地判定を、開始直後の一瞬だけ誤判定しないようにContext化する。
    airborneActionContextActive = true;
    airborneActionContextLeftGround = !IsGrounded();
}

void PlayerControllerComponent::EndAirborneActionContext()
{
    airborneActionContextActive = false;
    airborneActionContextLeftGround = false;
    jumpLoopResumeRequested = false;
}

// 空中行動からの復帰要求を一度だけ取り出し、新規ジャンプとの区別に使う。
bool PlayerControllerComponent::ConsumeJumpLoopResumeRequest()
{
    const bool requested_ = jumpLoopResumeRequested;
    jumpLoopResumeRequested = false;
    return requested_;
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetMoveInput() const
{
    // 入力軸をカメラ基準のワールド移動方向へ変換して、TPS操作として扱う。
    return GetCameraRelativeMoveDirection(gameObject.lock(), GetMoveInputAxis());
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetGuardMoveInput(
    const DirectX::XMFLOAT3& worldMoveInput_) const
{
    // ガード中は「前後左右」を敵方向基準に変換し、ロックオン時の防御移動を安定させる。
    const float inputLengthSquared_ =
        worldMoveInput_.x * worldMoveInput_.x +
        worldMoveInput_.z * worldMoveInput_.z;
    if (inputLengthSquared_ <= 0.0001f)
    {
        return { 0.0f, 0.0f, 0.0f };
    }

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform)
    {
        return { worldMoveInput_.x, 0.0f, worldMoveInput_.z };
    }

    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    const std::shared_ptr<CameraLockOnComponent> lockOn_ =
        ResolveCameraLockOnComponent();
    const std::shared_ptr<GameObject> target_ =
        lockOn_ && lockOn_->IsLockOnEnabled()
        ? lockOn_->GetLockOnTarget()
        : nullptr;
    if (target_ && target_->transform)
    {
        const DirectX::XMFLOAT3 ownerPosition_ =
            owner_->transform->GetWorldPosition();
        const DirectX::XMFLOAT3 targetPosition_ =
            target_->transform->GetWorldPosition();
        forward_ =
        {
            targetPosition_.x - ownerPosition_.x,
            0.0f,
            targetPosition_.z - ownerPosition_.z
        };
    }
    else
    {
        const DirectX::XMMATRIX ownerWorld_ =
            owner_->transform->GetWorldMatrix();
        DirectX::XMStoreFloat3(
            &forward_,
            DirectX::XMVector3Normalize(ownerWorld_.r[2]));
    }

    forward_ = NormalizeHorizontalDirection(forward_, { 0.0f, 0.0f, 1.0f });
    const DirectX::XMFLOAT3 right_
    {
        forward_.z,
        0.0f,
        -forward_.x
    };

    const DirectX::XMFLOAT3 normalizedInput_ = NormalizeHorizontalDirection(
        worldMoveInput_,
        { 0.0f, 0.0f, 0.0f });
    const float inputLength_ = (std::min)(std::sqrt(inputLengthSquared_), 1.0f);
    // 向きだけを基底へ射影し、アナログ入力の大きさは変換後も保持する。
    return
    {
        (normalizedInput_.x * right_.x + normalizedInput_.z * right_.z) * inputLength_,
        0.0f,
        (normalizedInput_.x * forward_.x + normalizedInput_.z * forward_.z) * inputLength_
    };
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetMoveInputAxis() const
{
    if (IsPlayerControlLockedByGameMode())
    {
        return { 0.0f, 0.0f, 0.0f };
    }

    auto& keyConfig_ = KeyConfig::Instance();
    DirectX::XMFLOAT3 input_
    {
        keyConfig_.GetAxis(PlayerInputAction::MOVE_X),
        0.0f,
        keyConfig_.GetAxis(PlayerInputAction::MOVE_Z)
    };

    const float lengthSquared_ = input_.x * input_.x + input_.z * input_.z;
    if (lengthSquared_ > 1.0f)
    {
        const float inverseLength_ = 1.0f / std::sqrt(lengthSquared_);
        input_.x *= inverseLength_;
        input_.z *= inverseLength_;
    }

    return input_;
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetCameraForwardDirection() const
{
    return ::GetCameraForwardDirection(gameObject.lock());
}

void PlayerControllerComponent::SetFacingDirection(const DirectX::XMFLOAT3& direction_)
{
    if (movement)
    {
        movement->SetFacingDirection(direction_, true);
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const float horizontalLengthSquared_ =
        direction_.x * direction_.x + direction_.z * direction_.z;
    if (horizontalLengthSquared_ <= 0.0001f) return;

    // Movementがない場合はTransformへ直接反映し、Dodge開始時の即時旋回にも使えるようにする。
    owner_->transform->SetForwardYawOnly(
        { direction_.x, 0.0f, direction_.z },
        1.0f,
        1.0f);
}

void PlayerControllerComponent::ApplyGuardFacingToTarget()
{
    // Guard中はLockOn対象へ向き続け、防御移動の前後左右がぶれないようにする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const std::shared_ptr<CameraLockOnComponent> lockOn_ =
        ResolveCameraLockOnComponent();
    if (!lockOn_ || !lockOn_->IsLockOnEnabled()) return;

    const std::shared_ptr<GameObject> target_ = lockOn_->GetLockOnTarget();
    if (!target_ || !target_->transform) return;

    const DirectX::XMFLOAT3 ownerPosition_ =
        owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 targetPosition_ =
        target_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 direction_
    {
        targetPosition_.x - ownerPosition_.x,
        0.0f,
        targetPosition_.z - ownerPosition_.z
    };

    if (movement)
    {
        movement->SetFacingDirection(direction_, false);
        return;
    }

    SetFacingDirection(direction_);
}

void PlayerControllerComponent::SetAttackAssistTarget(
    const std::shared_ptr<GameObject>& target_)
{
    attackAssistTarget = target_;
}

std::shared_ptr<GameObject> PlayerControllerComponent::ResolveSkillMotionWarpTarget() const
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::ResolveSkillMotionWarpTarget");
    const auto owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform || !owner_->manager) return nullptr;
    constexpr float MAXIMUM_DISTANCE = 6.0f;
    constexpr float MAXIMUM_HEIGHT = 4.0f;
    const auto position_ = owner_->transform->GetWorldPosition();
    const auto isValid_ = [&owner_, &position_](const std::shared_ptr<GameObject>& target_)
    {
        if (!target_ || target_ == owner_ || !target_->transform || target_->layer != LayerType::Enemy ||
            target_->IsPendingDestroy() || !target_->IsCombatReady()) return false;
        if (const auto health_ = target_->GetComponent<HealthComponent>()) if (health_->IsDead()) return false;
        const auto targetPosition_ = target_->transform->GetWorldPosition();
        const float x_ = targetPosition_.x - position_.x;
        const float z_ = targetPosition_.z - position_.z;
        return x_ * x_ + z_ * z_ <= MAXIMUM_DISTANCE * MAXIMUM_DISTANCE &&
            std::abs(targetPosition_.y - position_.y) <= MAXIMUM_HEIGHT;
    };
    if (const auto lockOn_ = ResolveCameraLockOnComponent())
        if (lockOn_->IsLockOnEnabled() && isValid_(lockOn_->GetLockOnTarget())) return lockOn_->GetLockOnTarget();
    // ロックオンしていない場合は、前方の敵だけを近い順で選ぶ。
    auto target_ = std::shared_ptr<GameObject>();
    float bestDistanceSquared_ = MAXIMUM_DISTANCE * MAXIMUM_DISTANCE;
    const auto forward_ = owner_->transform->GetFront();
    for (const auto& candidate_ : owner_->GetManager().GetObjects())
    {
        if (!isValid_(candidate_)) continue;
        const auto candidatePosition_ = candidate_->transform->GetWorldPosition();
        const float x_ = candidatePosition_.x - position_.x;
        const float z_ = candidatePosition_.z - position_.z;
        const float distanceSquared_ = x_ * x_ + z_ * z_;
        if (distanceSquared_ > bestDistanceSquared_ || x_ * forward_.x + z_ * forward_.z <= 0.0f) continue;
        target_ = candidate_;
        bestDistanceSquared_ = distanceSquared_;
    }
    return target_;
}

void PlayerControllerComponent::SetHorizontalVelocity(
    const DirectX::XMFLOAT3& dir_,
    float speed_,
    bool updateFacing_)
{
    // State内で直接Movementへ書かず、最後にApplyPendingMovementでまとめて反映する。
    const float inputLengthSq_ =
        dir_.x * dir_.x +
        dir_.z * dir_.z;

    hasPendingHorizontalVelocity = true;

    if (inputLengthSq_ < 0.0001f)
    {
        pendingHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
        hasPendingFacingDirection = false;
        return;
    }

    pendingHorizontalVelocity = { dir_.x * speed_, 0.0f, dir_.z * speed_ };
    if (updateFacing_)
    {
        pendingFacingDirection = { dir_.x, 0.0f, dir_.z };
        hasPendingFacingDirection = true;
    }
    else
    {
        hasPendingFacingDirection = false;
    }
}

// 向きの予約を変えず、既存の水平移動へ攻撃補正分の速度だけを加える。
void PlayerControllerComponent::AddHorizontalVelocityAssist(
    const DirectX::XMFLOAT3& dir_,
    float speed_)
{
    if (speed_ <= 0.0f) return;

    const float inputLengthSq_ =
        dir_.x * dir_.x +
        dir_.z * dir_.z;

    if (inputLengthSq_ < 0.0001f)
    {
        return;
    }

    const float inverseLength_ = 1.0f / std::sqrt(inputLengthSq_);

    const DirectX::XMFLOAT3 normalizedDirection_
    {
        dir_.x * inverseLength_,
        0.0f,
        dir_.z * inverseLength_
    };

    const DirectX::XMFLOAT3 assistVelocity_
    {
        normalizedDirection_.x * speed_,
        0.0f,
        normalizedDirection_.z * speed_
    };

    if (!hasPendingHorizontalVelocity)
    {
        pendingHorizontalVelocity = assistVelocity_;
        hasPendingHorizontalVelocity = true;
        return;
    }

    pendingHorizontalVelocity.x += assistVelocity_.x;
    pendingHorizontalVelocity.z += assistVelocity_.z;
}

void PlayerControllerComponent::ApplyPendingMovement()
{
    // Stateが予約した速度をUpdate終端でまとめてMovementへ渡し、State間の上書き順を安定させる。
    if (!hasPendingHorizontalVelocity) return;

    if (movement)
    {
        const float velocityLengthSq_ =
            pendingHorizontalVelocity.x * pendingHorizontalVelocity.x +
            pendingHorizontalVelocity.z * pendingHorizontalVelocity.z;

        if (velocityLengthSq_ < 0.0001f)
        {
            movement->StopHorizontalMovement();
        }
        else
        {
            movement->SetHorizontalVelocity(pendingHorizontalVelocity);
            if (hasPendingFacingDirection)
            {
                movement->SetFacingDirection(pendingFacingDirection, false);
            }
        }
    }
    else if (collider)
    {
        collider->velocity.x = pendingHorizontalVelocity.x;
        collider->velocity.z = pendingHorizontalVelocity.z;
    }

    hasPendingHorizontalVelocity = false;
    hasPendingFacingDirection = false;
}

bool PlayerControllerComponent::RequestAction(PLAYER_ACTION nextAction_)
{
    if (enemyExecutionCaptured || enemyExecutionRecovering) return false;
    if (guardBroken || (nextAction_ == PLAYER_ACTION::GUARD && guardRearmRequired)) return false;
    // 既存の未定義SkillA/Bは維持し、データ定義があるスキルのみ共通検証を行う。
    if ((PlayerSkillLoadout::GetSlotIndex(nextAction_) >= 0) &&
        !CanExecuteSkill(nextAction_)) return false;
    // Action遷移の入口。接地/空中/キャンセル/先行入力の条件をここで一元判定する。
    if (nextAction_ == PLAYER_ACTION::NONE)
    {
        return false;
    }
    if (nextAction_ == PLAYER_ACTION::JUMP && !IsGrounded())
    {
        return false;
    }
    if (IsAirAction(nextAction_) && !IsAirborneForAction())
    {
        return false;
    }
    if (IsDodgeAction(nextAction_) && !CanStartDodgeAction(nextAction_))
    {
        return false;
    }

    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (IsAirborneForAction() && IsGroundOnlyAction(nextAction_) && !GetSkillDefinition(nextAction_))
    {
        return false;
    }

    if (currentAction_ == nextAction_)
    {
        // 同じ弱攻撃入力は即時遷移せず、コンボ段の予約として保持する。
        const bool canQueueCombo_ =
            (currentAction_ == PLAYER_ACTION::WEAK_ATTACK ||
                currentAction_ == PLAYER_ACTION::AIR_WEAK_ATTACK) &&
            (actionRuntime.canBufferInput || actionRuntime.canCombo);
        if (canQueueCombo_)
        {
            actionRuntime.bufferedAction = nextAction_;
            actionRuntime.bufferedComboIndex = GetRequestedComboIndex(nextAction_);
        }
        return false;
    }

    const int requestedComboIndex_ = GetRequestedComboIndex(nextAction_);
    const bool isLocomotion_ =
        GetPlayerActionGroup(currentAction_) == PLAYER_ACTION_GROUP::LOCOMOTION;
    const bool nextUsesDodgeCancel_ = IsDodgeCancelAction(nextAction_);
    const bool canUseCancelTransition_ = nextUsesDodgeCancel_
        ? (actionRuntime.canDodgeCancel && HasDodgeCancelTransition(currentAction_, nextAction_))
        : (actionRuntime.canActionBranch && HasActionBranchTransition(currentAction_, nextAction_));
    const bool canChangeImmediately_ =
        currentAction_ == PLAYER_ACTION::NONE ||
        isLocomotion_ ||
        // 通常ガードからの受け流しは、アニメーションのキャンセル受付窓を待たずに開始する。
        (currentAction_ == PLAYER_ACTION::GUARD && nextAction_ == PLAYER_ACTION::JUST_GUARD) ||
        canUseCancelTransition_;

    if (!canChangeImmediately_)
    {
        // 現在Actionが閉じるまで待つ必要がある入力は、受付窓が開いている時だけBufferへ積む。
        if (actionRuntime.canBufferInput || actionRuntime.canCombo)
        {
            actionRuntime.bufferedAction = nextAction_;
            actionRuntime.bufferedComboIndex = requestedComboIndex_;
            if (nextAction_ == PLAYER_ACTION::COUNTER_ATTACK)
            {
                // 反撃入力を先行入力として受け付けた時点でTagを消費し、同じTagでの連続使用を防ぐ。
                ClearCounterTags();
            }
        }
        return false;
    }

    pendingComboIndex = requestedComboIndex_;
    stateMachine.ChangeState(nextAction_);
    if (nextAction_ == PLAYER_ACTION::COUNTER_ATTACK)
    {
        ClearCounterTags();
    }
    return true;
}

bool PlayerControllerComponent::HasCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_) const
{
    return HasDodgeCancelTransition(from_, to_) ||
        HasActionBranchTransition(from_, to_);
}

bool PlayerControllerComponent::AddCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    return IsDodgeCancelAction(to_)
        ? AddDodgeCancelTransition(from_, to_)
        : AddActionBranchTransition(from_, to_);
}

bool PlayerControllerComponent::RemoveCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    return IsDodgeCancelAction(to_)
        ? RemoveDodgeCancelTransition(from_, to_)
        : RemoveActionBranchTransition(from_, to_);
}

bool PlayerControllerComponent::HasDodgeCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_) const
{
    return std::any_of(
        dodgeCancelTransitions.begin(),
        dodgeCancelTransitions.end(),
        [from_, to_](const PlayerCancelTransition& transition_)
        {
            return transition_.from == from_ && transition_.to == to_;
        });
}

bool PlayerControllerComponent::AddDodgeCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    if (!IsDodgeAction(to_))
    {
        return false;
    }
    if (from_ == PLAYER_ACTION::NONE || to_ == PLAYER_ACTION::NONE ||
        from_ == to_ || HasDodgeCancelTransition(from_, to_))
    {
        return false;
    }

    dodgeCancelTransitions.push_back({ from_, to_ });
    return true;
}

bool PlayerControllerComponent::RemoveDodgeCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    const auto removeBegin_ = std::remove_if(
        dodgeCancelTransitions.begin(),
        dodgeCancelTransitions.end(),
        [from_, to_](const PlayerCancelTransition& transition_)
        {
            return transition_.from == from_ && transition_.to == to_;
        });
    if (removeBegin_ == dodgeCancelTransitions.end())
    {
        return false;
    }

    dodgeCancelTransitions.erase(removeBegin_, dodgeCancelTransitions.end());
    return true;
}

bool PlayerControllerComponent::HasActionBranchTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_) const
{
    return std::any_of(
        actionBranchTransitions.begin(),
        actionBranchTransitions.end(),
        [from_, to_](const PlayerCancelTransition& transition_)
        {
            return transition_.from == from_ && transition_.to == to_;
        });
}

bool PlayerControllerComponent::AddActionBranchTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    if (IsDodgeAction(to_))
    {
        return false;
    }
    if (from_ == PLAYER_ACTION::NONE || to_ == PLAYER_ACTION::NONE ||
        from_ == to_ || HasActionBranchTransition(from_, to_))
    {
        return false;
    }

    actionBranchTransitions.push_back({ from_, to_ });
    return true;
}

bool PlayerControllerComponent::RemoveActionBranchTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    const auto removeBegin_ = std::remove_if(
        actionBranchTransitions.begin(),
        actionBranchTransitions.end(),
        [from_, to_](const PlayerCancelTransition& transition_)
        {
            return transition_.from == from_ && transition_.to == to_;
        });
    if (removeBegin_ == actionBranchTransitions.end())
    {
        return false;
    }

    actionBranchTransitions.erase(removeBegin_, actionBranchTransitions.end());
    return true;
}

void PlayerControllerComponent::EnsureDefaultCancelTransitions()
{
    // Editor保存が空でも最低限の操作感を保つため、Player標準の派生ルールを補完する。
    constexpr PLAYER_ACTION jumpFromActions_[] =
    {
        PLAYER_ACTION::IDLE,
        PLAYER_ACTION::MOVE,
        PLAYER_ACTION::WEAK_ATTACK,
        PLAYER_ACTION::STRONG_ATTACK,
        PLAYER_ACTION::STRONG_CHARGE_ATTACK,
        PLAYER_ACTION::GUARD,
        PLAYER_ACTION::JUST_GUARD,
        PLAYER_ACTION::DODGE,
        PLAYER_ACTION::SKILL_A,
        PLAYER_ACTION::SKILL_B,
        PLAYER_ACTION::COUNTER_ATTACK
    };

    for (const PLAYER_ACTION action_ : jumpFromActions_)
    {
        AddActionBranchTransition(action_, PLAYER_ACTION::JUMP);
    }

    constexpr PLAYER_ACTION jumpAirActions_[] =
    {
        PLAYER_ACTION::AIR_WEAK_ATTACK,
        PLAYER_ACTION::AIR_STRONG_ATTACK,
        PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK,
        PLAYER_ACTION::AIR_DODGE
    };

    for (const PLAYER_ACTION action_ : jumpAirActions_)
    {
        AddActionBranchTransition(PLAYER_ACTION::JUMP, action_);
    }

    AddDodgeCancelTransition(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::STRONG_CHARGE_ATTACK, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::SKILL_A, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::SKILL_B, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::COUNTER_ATTACK, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::JUMP, PLAYER_ACTION::AIR_DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::AIR_WEAK_ATTACK, PLAYER_ACTION::AIR_DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::AIR_STRONG_ATTACK, PLAYER_ACTION::AIR_DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK, PLAYER_ACTION::AIR_DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::GUARD, PLAYER_ACTION::DODGE);
    AddDodgeCancelTransition(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::STRONG_CHARGE_ATTACK, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::SKILL_A, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::SKILL_B, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::COUNTER_ATTACK, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::GUARD, PLAYER_ACTION::JUST_GUARD);
    AddDodgeCancelTransition(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::DODGE);

    AddActionBranchTransition(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::STRONG_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::STRONG_CHARGE_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::SKILL_A);
    AddActionBranchTransition(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::SKILL_B);
    AddActionBranchTransition(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::SKILL_A);
    AddActionBranchTransition(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::SKILL_B);
    AddActionBranchTransition(PLAYER_ACTION::AIR_WEAK_ATTACK, PLAYER_ACTION::AIR_WEAK_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::AIR_WEAK_ATTACK, PLAYER_ACTION::AIR_STRONG_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::AIR_WEAK_ATTACK, PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::AIR_DODGE, PLAYER_ACTION::AIR_WEAK_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::AIR_DODGE, PLAYER_ACTION::AIR_STRONG_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::AIR_DODGE, PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::GUARD, PLAYER_ACTION::WEAK_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::GUARD, PLAYER_ACTION::STRONG_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::GUARD, PLAYER_ACTION::STRONG_CHARGE_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::GUARD, PLAYER_ACTION::COUNTER_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::DODGE, PLAYER_ACTION::COUNTER_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::AIR_DODGE, PLAYER_ACTION::COUNTER_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::COUNTER_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::WEAK_ATTACK);
    AddActionBranchTransition(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::STRONG_ATTACK);

    AddActionBranchTransition(PLAYER_ACTION::JUMP, PLAYER_ACTION::IDLE);
    AddActionBranchTransition(PLAYER_ACTION::JUMP, PLAYER_ACTION::MOVE);
}

std::shared_ptr<CameraLockOnComponent>
PlayerControllerComponent::ResolveCameraLockOnComponent() const
{
    // Player自身ではなくScene上のCamera側Componentを探し、LockOn情報を共有して使う。
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager) return nullptr;

    for (const std::shared_ptr<GameObject>& object_ : owner_->manager->GetObjects())
    {
        if (!object_) continue;
        if (const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            object_->GetComponent<CameraLockOnComponent>())
        {
            return lockOn_;
        }
    }

    return nullptr;
}

void PlayerControllerComponent::ToggleLockOn()
{
    const std::shared_ptr<CameraLockOnComponent> lockOn_ =
        ResolveCameraLockOnComponent();
    if (!lockOn_) return;

    lockOn_->ToggleLockOnForPlayer(GetGameObject());
}

void PlayerControllerComponent::SetCanCancel(bool enabled_)
{
    actionRuntime.canActionBranch = enabled_;
    SyncCancelGateBlackboard();
    if (enabled_)
    {
        TryConsumeBufferedActionForCancelGate(false);
    }
}

void PlayerControllerComponent::SetCanDodgeCancel(bool enabled_)
{
    actionRuntime.canDodgeCancel = enabled_;
    SyncCancelGateBlackboard();
    if (enabled_)
    {
        TryConsumeBufferedActionForCancelGate(true);
    }
}

void PlayerControllerComponent::SetCanActionBranch(bool enabled_)
{
    actionRuntime.canActionBranch = enabled_;
    SyncCancelGateBlackboard();
    if (enabled_)
    {
        TryConsumeBufferedActionForCancelGate(false);
    }
}

void PlayerControllerComponent::SetCanBufferInput(bool enabled_)
{
    actionRuntime.canBufferInput = enabled_;
    if (blackboard)
    {
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CAN_BUFFER_INPUT, actionRuntime.canBufferInput);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::CAN_BUFFER_INPUT, actionRuntime.canBufferInput);
    }
}

void PlayerControllerComponent::SetCanCombo(bool enabled_)
{
    actionRuntime.canCombo = enabled_;
    if (blackboard)
    {
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CAN_COMBO, actionRuntime.canCombo);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::CAN_COMBO, actionRuntime.canCombo);
    }
}

void PlayerControllerComponent::SyncCancelGateBlackboard()
{
    // Cancel受付状態はGame処理とAnimation条件の両方で参照するため、2カテゴリへ同時に送る。
    if (!blackboard) return;

    const bool legacyCanCancel_ = actionRuntime.canActionBranch;
    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_CANCEL, legacyCanCancel_);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_DODGE_CANCEL, actionRuntime.canDodgeCancel);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_ACTION_BRANCH, actionRuntime.canActionBranch);
    }
}

void PlayerControllerComponent::TryConsumeBufferedActionForCancelGate(bool dodgeGate_)
{
    // Timeline側から受付Gateが開いた瞬間に、保存済み入力を条件付きで消費する。
    if (actionRuntime.bufferedAction == PLAYER_ACTION::NONE)
    {
        return;
    }

    if (IsDodgeAction(actionRuntime.bufferedAction) != dodgeGate_)
    {
        return;
    }

    const PLAYER_ACTION nextAction_ = actionRuntime.bufferedAction;
    // 予約後に状況が変わっても、Cooldownや素材検証を飛ばしてSkillを開始しない。
    if ((PlayerSkillLoadout::GetSlotIndex(nextAction_) >= 0) && !CanExecuteSkill(nextAction_))
    {
        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
        return;
    }
    const PLAYER_ACTION currentAction_ = GetCurrentAction();

    if (dodgeGate_)
    {
        if (!actionRuntime.canDodgeCancel ||
            !HasDodgeCancelTransition(currentAction_, nextAction_) ||
            !CanStartDodgeAction(nextAction_))
        {
            return;
        }

        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
        pendingComboIndex = 0;
        stateMachine.ChangeState(nextAction_);
        return;
    }

    if (!actionRuntime.canActionBranch ||
        !HasActionBranchTransition(currentAction_, nextAction_))
    {
        return;
    }

    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    pendingComboIndex = GetRequestedComboIndex(nextAction_);
    stateMachine.ChangeState(nextAction_);
}

void PlayerControllerComponent::NotifyComboTransition()
{
    // Animation側のコンボ遷移通知で、保存済み弱攻撃入力を次コンボ段として確定する。
    SetCanBufferInput(false);
    SetCanCombo(false);
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (GetPlayerAttackType(currentAction_) != PLAYER_ATTACK_TYPE::WEAK ||
        actionRuntime.bufferedAction != currentAction_)
    {
        return;
    }

    pendingComboIndex = actionRuntime.bufferedComboIndex;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    stateMachine.ChangeState(currentAction_);
}

void PlayerControllerComponent::NotifyActionEnd()
{
    // 中断した攻撃の終了イベントで、死亡・復帰ステートを通常待機へ戻さない。
    if (GetCurrentAction() == PLAYER_ACTION::DEAD || GetCurrentAction() == PLAYER_ACTION::REVIVING) return;
    // 拘束と復帰の終了は専用グラフの状態で判定し、通常Actionの終了通知では解除しない。
    if (enemyExecutionCaptured || enemyExecutionRecovering) return;
    auto* skillState_ = stateMachine.GetCurrentStateAs<PlayerSkillState>();
    if (skillState_ && skillState_->IsActive() && !skillState_->IsFinishing())
    {
        // Animation/Sequenceからの通知はPhaseへ渡し、全体終了はStateに任せる。
        skillState_->NotifyPhaseEnd();
        return;
    }
    // Timeline駆動Actionの終了通知。Buffer消費後、空中/Guard/移動状態へ戻す。
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (!IsPlayerActionTimelineDriven(currentAction_) &&
        currentAction_ != PLAYER_ACTION::JUMP &&
        currentAction_ != PLAYER_ACTION::GUARD)
    {
        return;
    }

    if (IsPlayerActionTimelineDriven(currentAction_) && !(skillState_ && skillState_->IsFinishing()))
    {
        // 遷移前の再生から届いた終了通知で、新しい行動まで終了しないよう対応名を確認する。
        const auto owner_ = GetGameObject();
        const auto animationController_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
        const AnimState* currentState_ = animationController_ ? animationController_->GetCurrentState() : nullptr;
        const bool canUseFallback_ =
            timelineActionFallbackDuration > 0.0f &&
            timelineActionSafetyTimer >= timelineActionFallbackDuration;
        if (currentState_ &&
            !IsAnimationStateForAction(currentAction_, currentState_->name) &&
            !canUseFallback_)
        {
            return;
        }
    }

    actionRuntime.actionEnded = true;
    actionRuntime.canDodgeCancel = true;
    actionRuntime.canActionBranch = true;
    if (blackboard)
    {
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
    }
    SyncCancelGateBlackboard();

    if (GetPlayerAttackType(currentAction_) == PLAYER_ATTACK_TYPE::WEAK &&
        actionRuntime.bufferedAction == currentAction_)
    {
        // 同じ弱攻撃の予約はコンボ通知でのみ進め、行動終了後の新規攻撃としては消費しない。
        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
    }

    if (ConsumeBufferedAction())
    {
        return;
    }

    if (IsAirAction(currentAction_) ||
        (PlayerSkillLoadout::GetSlotIndex(currentAction_) >= 0 && IsAirborneForAction()))
    {
        const DirectX::XMFLOAT3 input_ = GetMoveInput();
        const bool hasMoveInput_ = input_.x != 0.0f || input_.z != 0.0f;
        if (IsGrounded() && hasMoveInput_)
        {
            stateMachine.ChangeState(PLAYER_ACTION::MOVE);
        }
        else
        {
            jumpLoopResumeRequested = true;
            stateMachine.ChangeState(PLAYER_ACTION::JUMP);
        }
        return;
    }

    if (ShouldEnterGuardAfterActionEnd())
    {
        SendGuardInputToBlackboard(true);
        stateMachine.ChangeState(PLAYER_ACTION::GUARD);
        return;
    }

    const DirectX::XMFLOAT3 input_ = GetMoveInput();
    const PLAYER_ACTION locomotionAction_ =
        (input_.x != 0.0f || input_.z != 0.0f)
        ? PLAYER_ACTION::MOVE
        : PLAYER_ACTION::IDLE;
    stateMachine.ChangeState(locomotionAction_);
}

void PlayerControllerComponent::NotifyJumpLoop()
{
    if (GetCurrentAction() != PLAYER_ACTION::JUMP)
    {
        return;
    }

    PlayerJumpState* jumpState_ = stateMachine.GetCurrentStateAs<PlayerJumpState>();
    if (!jumpState_)
    {
        return;
    }

    jumpState_->NotifyJumpLoopRequested();
}

void PlayerControllerComponent::NotifyJumpImpulse()
{
    if (GetCurrentAction() != PLAYER_ACTION::JUMP)
    {
        return;
    }

    PlayerJumpState* jumpState_ = stateMachine.GetCurrentStateAs<PlayerJumpState>();
    if (!jumpState_)
    {
        return;
    }

    jumpState_->ApplyJumpImpulseFromSequence();
}

void PlayerControllerComponent::ResetDeathPresentationState()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::ResetDeathPresentationState");
    CancelEnemyExecution(false);
    StopSkill();
    StopNormalGuard();
    stateMachine.ChangeState(PLAYER_ACTION::IDLE);
    actionRuntime = PlayerActionRuntime{};
    pendingComboIndex = 0;
    comboIndex = 0;
    hasPendingHorizontalVelocity = false;
    hasPendingFacingDirection = false;
    pendingHorizontalVelocity = {0.0f, 0.0f, 0.0f};
    SendMoveInputToBlackboard({0.0f, 0.0f, 0.0f});
    SendGuardInputToBlackboard(false);
    ResetStrongAttackInput();
    ClearCounterTags();
    ResetRuntimeArmor();
    if (movement)
    {
        movement->ResetMovementRequests();
        movement->ResetVerticalVelocity();
        movement->StopHorizontalMovementImmediately();
    }
    if (blackboard)
    {
        blackboard->SetBool("Anim", "DeathSequenceActive", false);
        blackboard->ResetTrigger("Anim", "PlayerDied");
        blackboard->ResetTrigger("Anim", "PlayerContinue");
        blackboard->ResetTrigger(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::ATTACK_ENTER);
        blackboard->ResetTrigger(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::ATTACK_ENTER);
    }
    const auto owner_ = GetGameObject();
    // 中断した攻撃のHitBoxを閉じ、起き上がり後に以前の攻撃判定が復活しないようにする。
    const auto stopAttack_ = [&](const auto& self_, const std::shared_ptr<GameObject>& object_) -> void
    {
        if (!object_) return;
        for (const auto& hit_ : object_->GetComponents<HitBox>()) if (hit_) hit_->Deactivate();
        if (object_->transform)
            for (const auto& child_ : object_->transform->children)
                if (child_) self_(self_, child_->gameObject.lock());
    };
    stopAttack_(stopAttack_, owner_);
    if (const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr)
    {
        // SkillやGuardの制御権だけを返し、再生先の選択はグラフへ任せる。
        animation_->SetSkillControlled(false);
        animation_->SetPresentationControlled(false);
    }
}

void PlayerControllerComponent::BeginDeathState()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::BeginDeathState");
    ResetDeathPresentationState();
    stateMachine.ChangeState(PLAYER_ACTION::DEAD);
}

void PlayerControllerComponent::BeginReviveState()
{
    if (GetCurrentAction() == PLAYER_ACTION::DEAD && IsDeathAnimationFinished())
        stateMachine.ChangeState(PLAYER_ACTION::REVIVING);
}

bool PlayerControllerComponent::IsDeathAnimationFinished() const
{
    const auto state_ = stateMachine.GetCurrentStateAs<PlayerDeathState>();
    return state_ && state_->IsAnimationFinished();
}

bool PlayerControllerComponent::IsReviveAnimationFinished() const
{
    const auto state_ = stateMachine.GetCurrentStateAs<PlayerReviveState>();
    return state_ && state_->IsAnimationFinished();
}

void PlayerControllerComponent::HandleHitReaction()
{
    if (GetCurrentAction() == PLAYER_ACTION::DEAD || GetCurrentAction() == PLAYER_ACTION::REVIVING) return;
    if (enemyExecutionCaptured || enemyExecutionRecovering)
    {
        const auto actor_ = GetGameObject();
        const auto health_ = actor_ ? actor_->GetComponent<HealthComponent>() : nullptr;
        if (health_ && health_->IsDead()) CancelEnemyExecution(false);
        else return; // HPの減少は維持し、拘束中の被弾で再生と位置固定を上書きしない。
    }
    // 突進の防御PhaseではHitStateへの強制遷移を止めるが、死亡時の処理は妨げない。
    const auto* skillState_ = stateMachine.GetCurrentStateAs<PlayerSkillState>();
    const auto owner_ = GetGameObject();
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    bool reacted_ = true;
    if (blackboard) blackboard->GetBool("Hit", "ImpactReacted", reacted_);
    // Armorで反応を抑えたHitはHP処理だけを維持し、弱い衝撃でHitStateへ移さない。
    if (!reacted_ && health_ && !health_->IsDead()) return;
    if (skillState_ && skillState_->PreventsHitReaction() && health_ && !health_->IsDead()) return;
    // 被弾時は受付状態と予約移動をすべて閉じ、HitStateへ強制的に移す。
    actionRuntime.canDodgeCancel = false;
    actionRuntime.canActionBranch = false;
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    pendingComboIndex = 0;
    comboIndex = 0;
    ClearCounterTags();
    hasPendingHorizontalVelocity = false;
    hasPendingFacingDirection = false;
    pendingHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
    stateMachine.ChangeState(PLAYER_ACTION::HIT);
}

// 基本設定を変更せず耐性を上書きし、実際の被弾判定を担うComponentにも同期する。
void PlayerControllerComponent::SetRuntimeArmor(float armor_)
{
    runtimeArmor = (std::max)(armor_, 0.0f);
    runtimeArmorOverrideActive = true;

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        if (const std::shared_ptr<HitReactionComponent> hitReaction_ =
            owner_->GetComponent<HitReactionComponent>())
        {
            hitReaction_->SetArmor(GetCurrentArmor());
        }
    }
}

// 一時的な耐性の上書きを閉じ、現在の基本設定を被弾判定へ戻す。
void PlayerControllerComponent::ResetRuntimeArmor()
{
    runtimeArmor = armor;
    runtimeArmorOverrideActive = false;

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        if (const std::shared_ptr<HitReactionComponent> hitReaction_ =
            owner_->GetComponent<HitReactionComponent>())
        {
            hitReaction_->SetArmor(GetCurrentArmor());
        }
    }
}

bool PlayerControllerComponent::ConsumeBufferedAction()
{
    // Action終了時に保存済み入力があれば、次Actionとして実行する。
    if (actionRuntime.bufferedAction == PLAYER_ACTION::NONE)
    {
        return false;
    }

    const PLAYER_ACTION nextAction_ = actionRuntime.bufferedAction;
    // 予約後に状況が変わっても、Cooldownや素材検証を飛ばしてSkillを開始しない。
    if ((PlayerSkillLoadout::GetSlotIndex(nextAction_) >= 0) && !CanExecuteSkill(nextAction_))
    {
        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
        return false;
    }
    if (IsDodgeAction(nextAction_) && !CanStartDodgeAction(nextAction_))
    {
        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
        return false;
    }

    pendingComboIndex = actionRuntime.bufferedComboIndex;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;

    stateMachine.ChangeState(nextAction_);

    return true;
}

void PlayerControllerComponent::EnterAction(PLAYER_ACTION action_)
{
    // StateMachineからAction開始時に呼ばれ、Runtime受付状態とBlackboard値を初期化する。
    const PLAYER_ACTION previousAction_ = previousAction;
    if (previousAction_ == PLAYER_ACTION::GUARD && action_ != PLAYER_ACTION::GUARD)
    {
        guardRearmRequired = KeyConfig::Instance().ActionPressed(PlayerInputAction::GUARD);
        StopNormalGuard();
    }
    previousAction = action_;
    if (previousAction_ == PLAYER_ACTION::DODGE &&
        action_ != PLAYER_ACTION::DODGE)
    {
        StartGroundDodgeCooldown();
    }
    if (action_ == PLAYER_ACTION::AIR_DODGE)
    {
        airDodgeConsumed = true;
    }
    if (action_ == PLAYER_ACTION::COUNTER_ATTACK)
    {
        // 反撃Actionが開始された時点で反撃Tagを消費し、次の入力へ持ち越さない。
        ClearCounterTags();
    }
    hasSentLocomotionPhase = false;
    if (locomotionPhaseAction != action_)
    {
        locomotionPhaseAction = action_;
        locomotionPhaseTimer = 0.0f;
    }
    runEndActive = action_ == PLAYER_ACTION::IDLE && previousAction_ == PLAYER_ACTION::MOVE;
    if (runEndActive)
    {
        runEndTimer = 0.0f;
    }

    if (action_ != PLAYER_ACTION::STRONG_ATTACK &&
        action_ != PLAYER_ACTION::STRONG_CHARGE_ATTACK &&
        action_ != PLAYER_ACTION::AIR_STRONG_ATTACK &&
        action_ != PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK)
    {
        ResetStrongAttackInput();
    }

    comboIndex = GetPlayerAttackType(action_) == PLAYER_ATTACK_TYPE::WEAK ? pendingComboIndex : 0;
    pendingComboIndex = 0;
    // 通知で進む行動の派生受付は閉じて開始し、アニメーション側の受付開始通知を待つ。
    actionRuntime.canDodgeCancel = !IsPlayerActionTimelineDriven(action_);
    actionRuntime.canActionBranch = !IsPlayerActionTimelineDriven(action_);
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    timelineActionSafetyTimer = 0.0f;

    SendActionToBlackboard(action_);
    if (action_ != PLAYER_ACTION::GUARD && blackboard) blackboard->SetBool("Guard", "Active", false);
    if (action_ == PLAYER_ACTION::GUARD)
    {
        guardBroken = false;
        SetNormalGuardEnabled(true);
        StartGuardVisual();
        const auto owner_ = GetGameObject();
        const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
        if (animation_ && !animation_->IsSkillControlled() && animation_->GetStates().count("Block_Walk_Start_Blend"))
            animation_->ChangeState("Block_Walk_Start_Blend", guardSettings.releaseBlendTime, true);
    }
    else if (previousAction_ == PLAYER_ACTION::GUARD &&
        (action_ == PLAYER_ACTION::IDLE || action_ == PLAYER_ACTION::MOVE))
    {
        const auto owner_ = GetGameObject();
        const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
        const std::string state_ = action_ == PLAYER_ACTION::MOVE ? "SS_Run" : "SS_Idle";
        // 見た目だけを補間し、Block_Endの終了待ちをゲームプレイへ持ち込まない。
        if (animation_ && !animation_->IsSkillControlled() && animation_->GetStates().count(state_))
            animation_->ChangeState(state_, guardSettings.releaseBlendTime);
    }
}

int PlayerControllerComponent::GetRequestedComboIndex(PLAYER_ACTION nextAction_) const
{
    // 弱攻撃だけコンボ段を持つため、同一Actionの予約時だけ次番号を計算する。
    if (nextAction_ != PLAYER_ACTION::WEAK_ATTACK &&
        nextAction_ != PLAYER_ACTION::AIR_WEAK_ATTACK)
    {
        return 0;
    }

    if (GetCurrentAction() == nextAction_ &&
        (actionRuntime.canBufferInput || actionRuntime.canCombo))
    {
        return (comboIndex + 1) % WEAK_ATTACK_COMBO_COUNT;
    }

    return 0;
}

bool PlayerControllerComponent::UpdateStrongAttackInput()
{
    // 強攻撃は押下時間で通常/溜めを分岐し、開始時の空中状態も保持する。
    KeyConfig& keyConfig_ = KeyConfig::Instance();

    if (keyConfig_.ActionPressedIn(PlayerInputAction::STRONG_ATTACK))
    {
        strongAttackHolding = true;
        strongAttackStartedAirborne = IsAirborneForAction();
        strongAttackHoldTimer = 0.0f;
    }

    if (!strongAttackHolding)
    {
        return false;
    }

    if (!keyConfig_.ActionPressed(PlayerInputAction::STRONG_ATTACK))
    {
        const PLAYER_ACTION attackAction_ =
            strongAttackHoldTimer >= strongAttackHoldThreshold
            ? (strongAttackStartedAirborne ? PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK : PLAYER_ACTION::STRONG_CHARGE_ATTACK)
            : (strongAttackStartedAirborne ? PLAYER_ACTION::AIR_STRONG_ATTACK : PLAYER_ACTION::STRONG_ATTACK);
        ResetStrongAttackInput();
        return RequestAction(attackAction_);
    }

    strongAttackHoldTimer += lastUpdateDeltaTime;
    if (strongAttackHoldTimer >= strongAttackHoldThreshold)
    {
        const bool startedAirborne_ = strongAttackStartedAirborne;
        ResetStrongAttackInput();
        return RequestAction(startedAirborne_
            ? PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK
            : PLAYER_ACTION::STRONG_CHARGE_ATTACK);
    }

    return false;
}

void PlayerControllerComponent::ResetStrongAttackInput()
{
    strongAttackHolding = false;
    strongAttackStartedAirborne = false;
    strongAttackHoldTimer = 0.0f;
}

bool PlayerControllerComponent::IsGroundOnlyAction(PLAYER_ACTION action_) const
{
    switch (action_)
    {
    case PLAYER_ACTION::WEAK_ATTACK:
    case PLAYER_ACTION::STRONG_ATTACK:
    case PLAYER_ACTION::STRONG_CHARGE_ATTACK:
    case PLAYER_ACTION::GUARD:
    case PLAYER_ACTION::JUST_GUARD:
    case PLAYER_ACTION::DODGE:
    case PLAYER_ACTION::SKILL_A:
    case PLAYER_ACTION::SKILL_B:
    case PLAYER_ACTION::SKILL_C:
    case PLAYER_ACTION::SKILL_D:
        return true;
    default:
        return false;
    }
}

bool PlayerControllerComponent::IsAirAction(PLAYER_ACTION action_) const
{
    switch (action_)
    {
    case PLAYER_ACTION::AIR_WEAK_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK:
    case PLAYER_ACTION::AIR_DODGE:
        return true;
    default:
        return false;
    }
}

bool PlayerControllerComponent::IsAirborneForAction() const
{
    // 空中Action中は接地状態だけでなく、ActionContextも見て空中扱いを継続する。
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    return !IsGrounded() &&
        (airborneActionContextActive ||
            IsAirAction(currentAction_) ||
            currentAction_ == PLAYER_ACTION::JUMP);
}

bool PlayerControllerComponent::IsPlayerControlLockedByGameMode() const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager) return false;

    const GameModeManager* gameModeManager_ = owner_->GetManager().GetGameModeManager();
    if (!gameModeManager_) return false;

    bool locked_ = false;
    if (gameModeManager_->GetGlobalBlackboard().GetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PLAYER_CONTROL_LOCKED,
        locked_))
    {
        return locked_;
    }

    return false;
}

bool PlayerControllerComponent::IsDodgeAction(PLAYER_ACTION action_) const
{
    return action_ == PLAYER_ACTION::DODGE ||
        action_ == PLAYER_ACTION::AIR_DODGE;
}

bool PlayerControllerComponent::IsDodgeCancelAction(PLAYER_ACTION action_) const
{
    return IsDodgeAction(action_) ||
        action_ == PLAYER_ACTION::JUST_GUARD;
}

// 地上回避の連続使用と、着地するまでの空中回避の再使用を別条件で制限する。
bool PlayerControllerComponent::CanStartDodgeAction(PLAYER_ACTION action_) const
{
    if (action_ == PLAYER_ACTION::DODGE)
    {
        return groundDodgeCooldownTimer <= 0.0f;
    }

    if (action_ == PLAYER_ACTION::AIR_DODGE)
    {
        return !airDodgeConsumed;
    }

    return true;
}

bool PlayerControllerComponent::ShouldEnterGuardAfterActionEnd() const
{
    if (guardRearmRequired) return false;
    if (IsPlayerControlLockedByGameMode())
    {
        return false;
    }

    // 攻撃やDodge終了時にGuard入力が押され続けていれば、Guardへ自然に復帰する。
    if (!IsGrounded() || IsAirborneForAction())
    {
        return false;
    }

    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (currentAction_ == PLAYER_ACTION::GUARD ||
        currentAction_ == PLAYER_ACTION::JUST_GUARD)
    {
        return false;
    }

    const PLAYER_ACTION_GROUP group_ = GetPlayerActionGroup(currentAction_);
    const bool isAttackAction_ =
        group_ == PLAYER_ACTION_GROUP::ATTACK ||
        currentAction_ == PLAYER_ACTION::STRONG_CHARGE_ATTACK;
    if (!isAttackAction_ &&
        currentAction_ != PLAYER_ACTION::DODGE)
    {
        return false;
    }

    return KeyConfig::Instance().ActionPressed(PlayerInputAction::GUARD);
}

// 時間で地上回避を再解禁し、接地で空中回避の使用回数を回復する。
void PlayerControllerComponent::UpdateDodgeRestrictions(float deltaTime_)
{
    groundDodgeCooldownTimer = (std::max)(
        groundDodgeCooldownTimer - (std::max)(deltaTime_, 0.0f),
        0.0f);

    if (IsGrounded())
    {
        airDodgeConsumed = false;
    }
}

void PlayerControllerComponent::StartGroundDodgeCooldown()
{
    groundDodgeCooldownTimer = (std::max)(groundDodgeCooldownDuration, 0.0f);
}

void PlayerControllerComponent::UpdateAirborneActionContext()
{
    // 一度空中へ出た後に接地したら、空中ActionContextを終了する。
    if (!airborneActionContextActive)
    {
        return;
    }

    if (!IsGrounded())
    {
        airborneActionContextLeftGround = true;
        return;
    }

    if (airborneActionContextLeftGround)
    {
        EndAirborneActionContext();
    }
}

void PlayerControllerComponent::ProcessActionInput()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::ProcessActionInput");
    // 優先度の高いActionから順に入力を解釈し、最初に成立したActionだけを要求する。
    if (IsPlayerControlLockedByGameMode())
    {
        SendGuardInputToBlackboard(false);
        ResetStrongAttackInput();
        return;
    }
    if (guardBroken) return;
    // Guard中は回避を複合Skillより先に判定し、既存のCooldownとキャンセル条件を使う。
    if (GetCurrentAction() == PLAYER_ACTION::GUARD && TryDodgeCancelInput()) return;

    auto& keyConfig_ = KeyConfig::Instance();
    const int skillSlot_ = PlayerInputResolver::ResolveSkillSlot(keyConfig_, skillInputs);
    const bool strongConsumed_ = PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::STRONG_ATTACK);
    if (strongConsumed_) ResetStrongAttackInput();
    if (skillSlot_ >= 0)
    {
        // 遷移拒否や未設定でも複合入力を消費し、通常攻撃との二重発動を防ぐ。
        ResetStrongAttackInput();
        RequestSkillSlot(static_cast<std::size_t>(skillSlot_));
        return;
    }
    const bool guardInput_ = keyConfig_.ActionPressed(PlayerInputAction::GUARD);
    SendGuardInputToBlackboard(guardInput_);

    if (keyConfig_.ActionPressedIn(PlayerInputAction::LOCK_ON))
    {
        ToggleLockOn();
    }

    const bool isAirborne_ = IsAirborneForAction();
    if (TryDodgeCancelInput()) return;
    if (!PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::JUST_GUARD) &&
        keyConfig_.ActionPressedIn(PlayerInputAction::JUST_GUARD) &&
        RequestAction(PLAYER_ACTION::JUST_GUARD)) return;
    if (!PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::DODGE) &&
        keyConfig_.ActionPressedIn(PlayerInputAction::DODGE) &&
        RequestAction(isAirborne_ ? PLAYER_ACTION::AIR_DODGE : PLAYER_ACTION::DODGE)) return;
    // 修飾中のジャンプ入力はスキルへ消費し、回復とジャンプの二重受付を防ぐ。
    if (!PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::JUMP) &&
        keyConfig_.ActionPressedIn(PlayerInputAction::JUMP) && RequestAction(PLAYER_ACTION::JUMP)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_A) && RequestAction(PLAYER_ACTION::SKILL_A)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_B) && RequestAction(PLAYER_ACTION::SKILL_B)) return;
    // KeyConfigに登録された単独SkillC入力も、複合入力と同じSlot2へ渡す。
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_C) && RequestSkillSlot(2)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_D) && RequestSkillSlot(3)) return;
    if (!PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::WEAK_ATTACK) &&
        keyConfig_.ActionPressedIn(PlayerInputAction::WEAK_ATTACK))
    {
        // 成功直後の反撃Tagが残っている間は、通常攻撃入力を反撃Actionへ差し替える。
        if (HasCounterAttackTag())
        {
            RequestAction(PLAYER_ACTION::COUNTER_ATTACK);
            return;
        }

        if (RequestAction(isAirborne_ ? PLAYER_ACTION::AIR_WEAK_ATTACK : PLAYER_ACTION::WEAK_ATTACK)) return;
    }
    if (!strongConsumed_ && keyConfig_.ActionPressedIn(PlayerInputAction::STRONG_ATTACK) &&
        HasCounterAttackTag())
    {
        // 反撃Tag中の強攻撃入力も溜め判定へ回さず、即座に反撃Actionとして消費する。
        RequestAction(PLAYER_ACTION::COUNTER_ATTACK);
        ResetStrongAttackInput();
        return;
    }
    if (!strongConsumed_ && actionRuntime.canActionBranch &&
        keyConfig_.ActionPressedIn(PlayerInputAction::STRONG_ATTACK) &&
        RequestAction(isAirborne_ ? PLAYER_ACTION::AIR_STRONG_ATTACK : PLAYER_ACTION::STRONG_ATTACK))
    {
        ResetStrongAttackInput();
        return;
    }
    if (!strongConsumed_ && UpdateStrongAttackInput()) return;

    if (guardInput_)
    {
        if (RequestAction(PLAYER_ACTION::GUARD)) return;
    }
    const DirectX::XMFLOAT3 input_ = GetMoveInput();
    const bool isLocomotion_ =
        GetPlayerActionGroup(GetCurrentAction()) == PLAYER_ACTION_GROUP::LOCOMOTION;
    if (isLocomotion_ &&
        (input_.x != 0.0f || input_.z != 0.0f) &&
        RequestAction(PLAYER_ACTION::MOVE))
    {
        return;
    }
}

void PlayerControllerComponent::RegisterPlayerAnimationActions()
{
    // Animation/ActionSequence側から呼ばれるPlayer制御用Commandを登録する。
    auto owner_ = GetGameObject();
    if (!owner_) return;

    // 演出の種類をStateへ埋め込まず、イベントから指定されたParticleを既存Managerへ渡す。
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    // 撃破直前の攻撃や先行入力を終了し、演出後に古いActionを再開させない。
    owner_->RegisterGlobalAction("ActorPresentation", "PlayerPresentation_Reset", [weakOwner_]()
    {
        BS_PROFILE_SCOPE("PlayerControllerComponent::PresentationReset");
        const auto actor_ = weakOwner_.lock();
        const auto controller_ = actor_ ? actor_->GetComponent<PlayerControllerComponent>() : nullptr;
        if (!controller_) return;
        controller_->StopNormalGuard();
        controller_->StopSkill();
        controller_->actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        controller_->stateMachine.ChangeState(PLAYER_ACTION::IDLE);
        controller_->ClearAttackAssistTarget();
        controller_->ResetStrongAttackInput();
    });
    owner_->RegisterAction("SkillEffect", "SkillEffect.PlayParticle", [weakOwner_]()
    {
        BS_PROFILE_SCOPE("PlayerSkillEffect::PlayParticle");
        const auto actor_ = weakOwner_.lock();
        if (!actor_ || !actor_->manager || !actor_->transform) return;
        const auto blackboard_ = actor_->GetComponent<BlackboardComponent>();
        ParticleSpawnRequest request_;
        if (!blackboard_ || !blackboard_->GetString("SkillEffect", "ParticleAsset", request_.assetPath) ||
            request_.assetPath.empty()) return;
        blackboard_->GetFloat("SkillEffect", "ParticleLifeTime", request_.lifeTime);
        float height_ = 0.0f;
        blackboard_->GetFloat("SkillEffect", "HeightOffset", height_);
        if (!std::isfinite(request_.lifeTime) || request_.lifeTime <= 0.0f || !std::isfinite(height_)) return;
        request_.position = actor_->transform->GetWorldPosition();
        request_.position.y += height_;
        actor_->GetManager().GetEffectManager().SpawnParticle(request_);
    });
    // Skillの終了・中断では武器Trailと既存Dodge用残像の生成をまとめて止める。
    owner_->RegisterAction("SkillEffect", "SkillEffect.StopTrails", [weakOwner_]()
    {
        BS_PROFILE_SCOPE("PlayerSkillEffect::StopTrails");
        const auto actor_ = weakOwner_.lock();
        if (!actor_) return;
        actor_->ExecuteAction("Weapon_Trail_Stop");
        actor_->ExecuteAction("AfterimageStop");
    });
    // Shield子Objectへ登録された攻撃停止Actionを、SkillのExitから呼べるようにする。
    owner_->RegisterAction("SkillEffect", "SkillEffect.StopShieldAttack", [weakOwner_]()
    {
        if (const auto actor_ = weakOwner_.lock()) actor_->ExecuteAction("Shield_DisableHitBox");
    });
    owner_->RegisterAction("PlayerReaction", "Player_Hit", [this]()
    {
        HandleHitReaction();
    });
    owner_->RegisterAction("PlayerArmor", "ArmorBase", [this]()
    {
        ResetRuntimeArmor();
    });
    owner_->RegisterAction("PlayerArmor", "ResetArmor", [this]()
    {
        ResetRuntimeArmor();
    });
    owner_->RegisterAction("PlayerArmor", "Armor10", [this]()
    {
        SetRuntimeArmor(10.0f);
    });
    owner_->RegisterAction("PlayerArmor", "Armor20", [this]()
    {
        SetRuntimeArmor(20.0f);
    });
    owner_->RegisterAction("PlayerReaction", "OnJustDodge", [this]()
    {
        NotifyJustDodge();
    });
    owner_->RegisterAction("PlayerReaction", "OnJustGuard", [this]()
    {
        StopGuardVisual();
        GrantJustGuardCounterTag();
    });
    const std::weak_ptr<PlayerControllerComponent> weakPlayer_ = owner_->GetComponent<PlayerControllerComponent>();
    owner_->RegisterAction("PlayerReaction", "OnGuardSucceeded", [weakPlayer_]()
    {
        if (const auto player_ = weakPlayer_.lock()) player_->HandleGuardImpact(false);
    });
    owner_->RegisterAction("PlayerReaction", "OnGuardBreak", [weakPlayer_]()
    {
        if (const auto player_ = weakPlayer_.lock()) player_->HandleGuardImpact(true);
    });
    owner_->RegisterAction("PlayerReaction", "Player_GuardReactionEnd", [weakPlayer_]()
    {
        if (const auto player_ = weakPlayer_.lock())
            if (player_->GetCurrentAction() == PLAYER_ACTION::GUARD && player_->guardReactionActive)
                player_->guardReactionEndRequested = true;
    });
    owner_->RegisterAction("PlayerCancel", "CanCancelOn", [this]()
        {
            SetCanCancel(true);
        });
    owner_->RegisterAction("PlayerCancel", "CanCancelOff", [this]()
        {
            SetCanCancel(false);
        });
    owner_->RegisterAction("PlayerCancel", "EnableDodgeCancel", [this]()
        {
            SetCanDodgeCancel(true);
        });
    owner_->RegisterAction("PlayerCancel", "CanDodgeOn", [this]()
        {
            SetCanDodgeCancel(true);
        });
    owner_->RegisterAction("PlayerCancel", "DisableDodgeCancel", [this]()
        {
            SetCanDodgeCancel(false);
        });
    owner_->RegisterAction("PlayerCancel", "CanDodgeOff", [this]()
        {
            SetCanDodgeCancel(false);
        });
    owner_->RegisterAction("PlayerCancel", "EnableActionBranch", [this]()
        {
            SetCanActionBranch(true);
        });
    owner_->RegisterAction("PlayerCancel", "DisableActionBranch", [this]()
        {
            SetCanActionBranch(false);
        });
    owner_->RegisterAction("PlayerBuffer", "InputBufferOn", [this]()
        {
            SetCanBufferInput(true);
        });
    owner_->RegisterAction("PlayerBuffer", "InputBufferOff", [this]()
        {
            SetCanBufferInput(false);
        });
    owner_->RegisterAction("PlayerCombo", "CanComboOn", [this]()
        {
            SetCanCombo(true);
        });
    owner_->RegisterAction("PlayerCombo", "EnableComboInput", [this]()
        {
            SetCanCombo(true);
        });
    owner_->RegisterAction("PlayerCombo", "CanComboOff", [this]()
        {
            SetCanCombo(false);
        });
    owner_->RegisterAction("PlayerCombo", "DisableComboInput", [this]()
        {
            SetCanCombo(false);
        });
    owner_->RegisterAction("PlayerCombo", "ComboTransition", [this]()
        {
            NotifyComboTransition();
        });
    owner_->RegisterAction("PlayerCombo", "CommitBufferedCombo", [this]()
        {
            NotifyComboTransition();
        });
    owner_->RegisterAction("PlayerState", "ActionEnd", [this]()
        {
            NotifyActionEnd();
        });
    owner_->RegisterAction("PlayerState", "NotifyActionEnd", [this]()
        {
            NotifyActionEnd();
        });
    owner_->RegisterAction("PlayerState", "Player_Jump", [this]()
        {
            RequestAction(PLAYER_ACTION::JUMP);
        });
    owner_->RegisterAction("PlayerJump", "JumpStart", [this]()
        {
            RequestAction(PLAYER_ACTION::JUMP);
        });
    owner_->RegisterAction("PlayerJump", "ApplyJumpImpulse", [this]()
        {
            NotifyJumpImpulse();
        });
    owner_->RegisterAction("PlayerJump", "JumpImpulse", [this]()
        {
            NotifyJumpImpulse();
        });
    owner_->RegisterAction("PlayerJump", "JumpLoop", [this]()
        {
            NotifyJumpLoop();
        });
    owner_->RegisterAction("PlayerJump", "NotifyJumpLoop", [this]()
        {
            NotifyJumpLoop();
        });

}

void PlayerControllerComponent::SendActionToBlackboard(PLAYER_ACTION action_)
{
    // 現在ActionをGame側とAnimation側のBlackboardへ同期し、遷移条件を揃える。
    if (!blackboard)
    {
        auto owner_ = GetGameObject();
        blackboard = owner_ ? owner_->GetOrAddComponent<BlackboardComponent>() : nullptr;
    }

    if (!blackboard) return;

    const PLAYER_ACTION_GROUP group_ = GetPlayerActionGroup(action_);
    const PLAYER_ATTACK_TYPE attackType_ = GetPlayerAttackType(action_);
    const bool isAirAction_ = IsAirAction(action_);
    const bool isAirAttack_ =
        action_ == PLAYER_ACTION::AIR_WEAK_ATTACK ||
        action_ == PLAYER_ACTION::AIR_STRONG_ATTACK ||
        action_ == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK;
    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard->SetInt(category_, PlayerBlackboardKey::ACTION, static_cast<int>(action_));
        blackboard->SetInt(category_, PlayerBlackboardKey::ACTION_GROUP, static_cast<int>(group_));
        blackboard->SetInt(category_, PlayerBlackboardKey::ATTACK_TYPE, static_cast<int>(attackType_));
        blackboard->SetInt(category_, PlayerBlackboardKey::COMBO_INDEX, comboIndex);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_IDLE, action_ == PLAYER_ACTION::IDLE);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_MOVE, action_ == PLAYER_ACTION::MOVE);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_LOCOMOTION, group_ == PLAYER_ACTION_GROUP::LOCOMOTION);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_ATTACK, group_ == PLAYER_ACTION_GROUP::ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_GUARD, group_ == PLAYER_ACTION_GROUP::GUARD);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUST_GUARD, action_ == PLAYER_ACTION::JUST_GUARD);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_DODGE, group_ == PLAYER_ACTION_GROUP::DODGE);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP, group_ == PLAYER_ACTION_GROUP::JUMP);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_ACTION, isAirAction_);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_ATTACK, isAirAttack_);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_WEAK_ATTACK, action_ == PLAYER_ACTION::AIR_WEAK_ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_STRONG_ATTACK, action_ == PLAYER_ACTION::AIR_STRONG_ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_STRONG_CHARGE_ATTACK, action_ == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_DODGE, action_ == PLAYER_ACTION::AIR_DODGE);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_SKILL,
            attackType_ == PLAYER_ATTACK_TYPE::SKILL_A || attackType_ == PLAYER_ATTACK_TYPE::SKILL_B ||
            attackType_ == PLAYER_ATTACK_TYPE::SKILL_C || attackType_ == PLAYER_ATTACK_TYPE::SKILL_D);
        const bool legacyCanCancel_ = actionRuntime.canActionBranch;
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_CANCEL, legacyCanCancel_);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_DODGE_CANCEL, actionRuntime.canDodgeCancel);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_ACTION_BRANCH, actionRuntime.canActionBranch);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_BUFFER_INPUT, actionRuntime.canBufferInput);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_COMBO, actionRuntime.canCombo);
        blackboard->SetBool(category_, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
    }

    const bool shouldEnterAttack_ =
        group_ == PLAYER_ACTION_GROUP::ATTACK &&
        comboIndex == 0;
    blackboard->SetBool(
        PlayerBlackboardCategory::ANIM,
        PlayerBlackboardKey::ATTACK_ENTER,
        shouldEnterAttack_);

    SendMoveInputToBlackboard(GetMoveInput());
}

void PlayerControllerComponent::UpdateLocomotionAnimationPhase(float deltaTime_)
{
    // 移動開始/Loop/停止の見た目用Phaseを時間で切り替える。
    locomotionPhaseTimer += (std::max)(deltaTime_, 0.0f);

    PLAYER_LOCOMOTION_PHASE phase_ = PLAYER_LOCOMOTION_PHASE::IDLE;
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (currentAction_ == PLAYER_ACTION::JUMP)
    {
        return;
    }
    else if (currentAction_ == PLAYER_ACTION::MOVE)
    {
        phase_ = locomotionPhaseTimer <= runStartDuration
            ? PLAYER_LOCOMOTION_PHASE::RUN_START
            : PLAYER_LOCOMOTION_PHASE::RUN_LOOP;
        runEndActive = false;
    }
    else if (currentAction_ == PLAYER_ACTION::IDLE && runEndActive)
    {
        runEndTimer += (std::max)(deltaTime_, 0.0f);
        phase_ = PLAYER_LOCOMOTION_PHASE::RUN_END;
        if (runEndTimer >= runEndDuration)
        {
            runEndActive = false;
        }
    }

    SendLocomotionPhaseToBlackboard(phase_);
}

void PlayerControllerComponent::SendLocomotionPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE phase_)
{
    // 同じPhaseの再送を避けつつ、Animation遷移に必要なBoolもまとめて更新する。
    if (!blackboard) return;
    if (hasSentLocomotionPhase && lastSentLocomotionPhase == phase_)
    {
        return;
    }

    hasSentLocomotionPhase = true;
    lastSentLocomotionPhase = phase_;

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard->SetInt(
            category_,
            PlayerBlackboardKey::LOCOMOTION_PHASE,
            static_cast<int>(phase_));
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_RUN_START,
            phase_ == PLAYER_LOCOMOTION_PHASE::RUN_START);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_RUN_LOOP,
            phase_ == PLAYER_LOCOMOTION_PHASE::RUN_LOOP);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_RUN_END,
            phase_ == PLAYER_LOCOMOTION_PHASE::RUN_END);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_JUMP,
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP ||
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_START ||
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_DOUBLE ||
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_LOOP ||
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_END);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_JUMP_START,
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_START);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_JUMP_DOUBLE,
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_DOUBLE);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_JUMP_LOOP,
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_LOOP);
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_JUMP_END,
            phase_ == PLAYER_LOCOMOTION_PHASE::JUMP_END);
    }
}

void PlayerControllerComponent::SendGroundedToBlackboard()
{
    // 接地状態は変化した時だけBlackboardへ送る。
    if (!blackboard) return;

    const bool isGrounded_ = IsGrounded();
    if (hasSentGrounded && lastSentGrounded == isGrounded_)
    {
        return;
    }

    hasSentGrounded = true;
    lastSentGrounded = isGrounded_;

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard->SetBool(
            category_,
            PlayerBlackboardKey::IS_GROUNDED,
            isGrounded_);
    }
}

bool PlayerControllerComponent::ForceChangeAction(PLAYER_ACTION action_)
{
    if (action_ == PLAYER_ACTION::NONE)
    {
        return false;
    }

    // Debug切り替えなので、入力Bufferやコンボ待ちを全部リセットする。
    actionRuntime.canDodgeCancel = true;
    actionRuntime.canActionBranch = true;
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;

    pendingComboIndex = 0;
    comboIndex = 0;

    // ChangeState内で現在StateのExitが呼ばれる。
    stateMachine.ChangeState(action_);
    return true;
}

void PlayerControllerComponent::SendMoveInputToBlackboard(const DirectX::XMFLOAT3& input_)
{
    if (!blackboard) return;

    blackboard->SetVector2(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::MOVE_INPUT, { input_.x, input_.z });
    blackboard->SetVector2(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::MOVE_INPUT, { input_.x, input_.z });
}

void PlayerControllerComponent::SendGuardMoveInputToBlackboard(const DirectX::XMFLOAT3& input_)
{
    if (!blackboard) return;

    blackboard->SetVector2(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::GUARD_MOVE_INPUT, { input_.x, input_.z });
    blackboard->SetVector2(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::GUARD_MOVE_INPUT, { input_.x, input_.z });
}

void PlayerControllerComponent::SendGuardInputToBlackboard(bool guardInput_)
{
    if (!blackboard) return;

    blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::GUARD_INPUT, guardInput_);
    blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::GUARD_INPUT, guardInput_);
}

// 成功確定後だけ演出を開始し、復帰は既存TimeManagerと音声Mixerに任せる。
void PlayerControllerComponent::PlayJustSuccessPresentation()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::PlayJustSuccessPresentation");
    const auto owner_ = GetGameObject();
    if (!justSuccessPresentationEnabled || !owner_ || !owner_->manager) return;
    // プレイヤーの操作速度を保ちながら、敵と弾の動きを短時間だけ遅くする。
    constexpr uint32_t SLOW_LAYERS = static_cast<uint32_t>(TarGetLayerType::Enemy) |
        static_cast<uint32_t>(TarGetLayerType::EnemyBullet) | static_cast<uint32_t>(TarGetLayerType::PlayerBullet) |
        static_cast<uint32_t>(TarGetLayerType::Environment);
    if (const auto time_ = owner_->GetManager().GetTimeManager())
        time_->PlayTimedScale(SLOW_LAYERS, justSuccessTimeScale, 0.025f,
            justSuccessHoldDuration, justSuccessReturnDuration);
    AudioManager::Instance().PlayTimedMuffle(0.025f + justSuccessHoldDuration + justSuccessReturnDuration,
        justSuccessMuffleCutoff);
}

void PlayerControllerComponent::GrantJustDodgeCounterTag()
{
    // ジャスト回避成功後、通常攻撃入力を反撃へ差し替えるための短時間Tagを付与する。
    PlayJustSuccessPresentation();
    justDodgeCounterTagTimer = (std::max)(counterTagDuration, 0.0f);
    PromoteBufferedAttackToCounterAttack();
    SyncCounterTagsToBlackboard();
}

void PlayerControllerComponent::GrantJustGuardCounterTag()
{
    // ジャストガード成功後、反撃Tagと回避派生用の受付状態を同時に開く。
    PlayJustSuccessPresentation();
    justGuardCounterTagTimer = (std::max)(counterTagDuration, 0.0f);
    SetCanDodgeCancel(true);
    PromoteBufferedAttackToCounterAttack();
    SyncCounterTagsToBlackboard();
}

bool PlayerControllerComponent::HasCounterAttackTag() const
{
    return justDodgeCounterTagTimer > 0.0f || justGuardCounterTagTimer > 0.0f;
}

void PlayerControllerComponent::PromoteBufferedAttackToCounterAttack()
{
    const PLAYER_ACTION bufferedAction_ = actionRuntime.bufferedAction;
    const bool isBufferedGroundAttack_ =
        bufferedAction_ == PLAYER_ACTION::WEAK_ATTACK ||
        bufferedAction_ == PLAYER_ACTION::STRONG_ATTACK ||
        bufferedAction_ == PLAYER_ACTION::STRONG_CHARGE_ATTACK ||
        bufferedAction_ == PLAYER_ACTION::AIR_WEAK_ATTACK ||
        bufferedAction_ == PLAYER_ACTION::AIR_STRONG_ATTACK ||
        bufferedAction_ == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK;
    if (!isBufferedGroundAttack_)
    {
        return;
    }

    // 反撃Tag付与前に押されていた通常攻撃は、Tag成立後の反撃入力として扱う。
    actionRuntime.bufferedAction = PLAYER_ACTION::COUNTER_ATTACK;
    actionRuntime.bufferedComboIndex = 0;
    if (actionRuntime.canActionBranch)
    {
        TryConsumeBufferedActionForCancelGate(false);
    }
}

// 反撃の受付時間を進め、有効・無効が切り替わったときだけBlackboardを更新する。
void PlayerControllerComponent::UpdateCounterTags(float deltaTime_)
{
    const float previousJustDodgeTimer_ = justDodgeCounterTagTimer;
    const float previousJustGuardTimer_ = justGuardCounterTagTimer;
    const float elapsedTime_ = (std::max)(deltaTime_, 0.0f);

    if (justDodgeCounterTagTimer > 0.0f)
    {
        justDodgeCounterTagTimer = (std::max)(justDodgeCounterTagTimer - elapsedTime_, 0.0f);
    }
    if (justGuardCounterTagTimer > 0.0f)
    {
        justGuardCounterTagTimer = (std::max)(justGuardCounterTagTimer - elapsedTime_, 0.0f);
    }

    if ((previousJustDodgeTimer_ > 0.0f) != (justDodgeCounterTagTimer > 0.0f) ||
        (previousJustGuardTimer_ > 0.0f) != (justGuardCounterTagTimer > 0.0f))
    {
        SyncCounterTagsToBlackboard();
    }
}

// 回避・ガード別の反撃受付を、通常入力とアニメーションが参照するカテゴリへ同期する。
void PlayerControllerComponent::SyncCounterTagsToBlackboard()
{
    if (!blackboard) return;

    const bool hasJustDodgeCounterTag_ = justDodgeCounterTagTimer > 0.0f;
    const bool hasJustGuardCounterTag_ = justGuardCounterTagTimer > 0.0f;
    const bool hasCounterTag_ = hasJustDodgeCounterTag_ || hasJustGuardCounterTag_;
    // 両方の受付が残っている場合も個別フラグは保持し、表示元の名前はガードを優先する。
    const std::string source_ = hasJustGuardCounterTag_
        ? "JustGuard"
        : (hasJustDodgeCounterTag_ ? "JustDodge" : "");

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_COUNTER, hasCounterTag_);
        blackboard->SetBool(category_, PlayerBlackboardKey::COUNTER_TAG, hasCounterTag_);
        blackboard->SetString(category_, PlayerBlackboardKey::COUNTER_TAG_SOURCE, source_);
        blackboard->SetBool(category_, PlayerBlackboardKey::DODGED_COUNTER_TAG, hasJustDodgeCounterTag_);
        blackboard->SetBool(category_, PlayerBlackboardKey::GUARDED_COUNTER_TAG, hasJustGuardCounterTag_);
        blackboard->SetBool(category_, PlayerBlackboardKey::JUST_GUARD_COUNTER_TAG, hasJustGuardCounterTag_);
    }

    blackboard->SetBool("Dodge", PlayerBlackboardKey::CAN_COUNTER, hasJustDodgeCounterTag_);
    blackboard->SetBool("Dodge", PlayerBlackboardKey::DODGED_COUNTER_TAG, hasJustDodgeCounterTag_);
    blackboard->SetBool("Guard", PlayerBlackboardKey::CAN_COUNTER, hasJustGuardCounterTag_);
    blackboard->SetBool("Guard", PlayerBlackboardKey::GUARDED_COUNTER_TAG, hasJustGuardCounterTag_);
    blackboard->SetBool("Guard", PlayerBlackboardKey::JUST_GUARD_COUNTER_TAG, hasJustGuardCounterTag_);
}

// 反撃の開始・予約時に両方の受付を消費し、同じ成功通知の使い回しを防ぐ。
void PlayerControllerComponent::ClearCounterTags()
{
    justDodgeCounterTagTimer = 0.0f;
    justGuardCounterTagTimer = 0.0f;
    SyncCounterTagsToBlackboard();
}

void PlayerControllerComponent::SendJumpPhaseToBlackboard(
    PLAYER_LOCOMOTION_PHASE phase_)
{
    if (phase_ != PLAYER_LOCOMOTION_PHASE::JUMP_START &&
        phase_ != PLAYER_LOCOMOTION_PHASE::JUMP_DOUBLE &&
        phase_ != PLAYER_LOCOMOTION_PHASE::JUMP_LOOP &&
        phase_ != PLAYER_LOCOMOTION_PHASE::JUMP_END)
    {
        phase_ = PLAYER_LOCOMOTION_PHASE::JUMP;
    }

    SendLocomotionPhaseToBlackboard(phase_);
}

bool PlayerControllerComponent::TryDodgeCancelInput()
{
    const auto& keyConfig_ = KeyConfig::Instance();
    if (PlayerInputResolver::ResolveSkill(keyConfig_, skillInputs) != PLAYER_ACTION::NONE ||
        PlayerInputResolver::IsActionConsumed(keyConfig_, skillInputs, PlayerInputAction::DODGE)) return false;
    if (IsPlayerControlLockedByGameMode())
    {
        return false;
    }

    // Dodge受付が開いている時だけ、現在ActionからDodge派生できるか確認する。
    if (!actionRuntime.canDodgeCancel)
    {
        return false;
    }

    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    const bool currentAirAction_ =
        IsAirAction(currentAction_) ||
        currentAction_ == PLAYER_ACTION::JUMP;
    const bool shouldUseAirDodge_ =
        !IsGrounded() &&
        (IsAirborneForAction() || currentAirAction_);
    const PLAYER_ACTION dodgeAction_ = shouldUseAirDodge_
        ? PLAYER_ACTION::AIR_DODGE
        : PLAYER_ACTION::DODGE;
    if (!HasDodgeCancelTransition(currentAction_, dodgeAction_) ||
        !CanStartDodgeAction(dodgeAction_))
    {
        return false;
    }

    if (IsDodgeAction(actionRuntime.bufferedAction))
    {
        TryConsumeBufferedActionForCancelGate(true);
        return GetCurrentAction() == dodgeAction_;
    }

    if (!KeyConfig::Instance().ActionPressed(PlayerInputAction::DODGE))
    {
        return false;
    }

    return RequestAction(dodgeAction_);
}

void PlayerControllerComponent::NotifyJustDodge()
{
    // JustDodge判定成功を現在のDodgeStateへ橋渡しする。
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        std::string("[JustDodge] PlayerController NotifyJustDodge currentAction=")
        + GetPlayerActionName(currentAction_));

    if (currentAction_ != PLAYER_ACTION::DODGE &&
        currentAction_ != PLAYER_ACTION::AIR_DODGE)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] PlayerController rejected: current action is not Dodge.");
        return;
    }

    PlayerDodgeState* dodgeState_ =
        stateMachine.GetCurrentStateAs<PlayerDodgeState>();
    if (!dodgeState_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] PlayerController rejected: current state is not PlayerDodgeState.");
        return;
    }

    dodgeState_->NotifyJustDodgeSucceeded();
}

bool PlayerControllerComponent::IsJustDodgeColliderActive() const
{
    return justDodgeCollider && justDodgeCollider->IsEnabled();
}


void PlayerControllerComponent::Gui()
{
    ImGui::Text("Enemy execution: %s / %.2f sec", enemyExecutionRecoilActive ? "RECOIL" : enemyExecutionCaptured ? "CAPTURED" : enemyExecutionRecovering ? "RECOVERING" : "OFF", enemyExecutionElapsed);
    // Runtime状態と調整値をEditor上で確認・変更するためのDebug UI。
    ImGui::Text("State: %s", stateMachine.GetCurrentStateName());
    ImGui::Text("Current Action: %s", GetPlayerActionName(GetCurrentAction()));

    // 共通Stateの実行状況を既存Player GUIへまとめて表示する。
    const auto* skillState_ = stateMachine.GetCurrentStateAs<PlayerSkillState>();
    ImGui::SeparatorText("Skill");
    ImGui::Text("Skill Active: %s", skillState_ && skillState_->IsActive() ? "true" : "false");
    ImGui::Text("Current Skill: %s", skillState_ ? skillState_->GetSkillName().c_str() : "");
    ImGui::Text("Current Phase: %d", skillState_ ? skillState_->GetPhaseIndex() : -1);
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto* currentAnimation_ = animation_ ? animation_->GetCurrentState() : nullptr;
    ImGui::Text("Current Animation: %s", currentAnimation_ ? currentAnimation_->name.c_str() : "None");
    // プレイヤーのInspectorから、通常攻撃とスキルの補正を同じスイッチで比較する。
    if (const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr) warp_->DrawComparisonGui();
    for (const auto& binding_ : skillInputs)
        ImGui::Text("Skill Input: %s + %s -> SkillSlot%d (priority %d)", binding_.modifierAction.c_str(),
            binding_.triggerAction.c_str(), binding_.slotIndex, binding_.priority);
    ImGui::Text("Skill Definitions: %s", skillDefinitionsPath.c_str());
    for (const auto& definition_ : skillDefinitions)
        ImGui::Text("%s: %.2f / %.2f sec", definition_.name.c_str(),
            GetRemainingSkillCooldown(definition_.name), definition_.cooldown);
    DrawSkillLoadoutGui();
    if (ImGui::Button("Reset Skill Cooldowns")) ResetSkillCooldowns();

    ImGui::SeparatorText("Runtime");

    ImGui::Text("Dodge Cancel Gate: %s", actionRuntime.canDodgeCancel ? "true" : "false");
    ImGui::Text("Action Branch Gate: %s", actionRuntime.canActionBranch ? "true" : "false");
    ImGui::Text("Can Buffer Input: %s", actionRuntime.canBufferInput ? "true" : "false");
    ImGui::Text("Can Combo: %s", actionRuntime.canCombo ? "true" : "false");
    ImGui::Text("Action Ended: %s", actionRuntime.actionEnded ? "true" : "false");
    ImGui::Text("Combo Index: %d", comboIndex);
    ImGui::Text("Buffered Action: %s", GetPlayerActionName(actionRuntime.bufferedAction));
    ImGui::Text("Ground Dodge Cooldown: %.2f", groundDodgeCooldownTimer);
    ImGui::Text("Air Dodge Consumed: %s", airDodgeConsumed ? "true" : "false");

    ImGui::SeparatorText("Cancel Transitions");
    ImGui::Text("Dodge Cancel Routes: %zu", dodgeCancelTransitions.size());
    ImGui::Text("Action Branch Routes: %zu", actionBranchTransitions.size());
    ImGui::TextWrapped("Edit routes in the Player State Graph workspace.");

    ImGui::SeparatorText("Debug Change Action");

    if (ImGui::BeginCombo("Action", GetPlayerActionName(debugSelectedAction)))
    {
        constexpr PLAYER_ACTION actions_[] =
        {
            PLAYER_ACTION::IDLE,
            PLAYER_ACTION::MOVE,
            PLAYER_ACTION::WEAK_ATTACK,
            PLAYER_ACTION::STRONG_ATTACK,
            PLAYER_ACTION::STRONG_CHARGE_ATTACK,
            PLAYER_ACTION::GUARD,
            PLAYER_ACTION::JUST_GUARD,
            PLAYER_ACTION::DODGE,
            PLAYER_ACTION::JUMP,
            PLAYER_ACTION::AIR_WEAK_ATTACK,
            PLAYER_ACTION::AIR_STRONG_ATTACK,
            PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK,
            PLAYER_ACTION::AIR_DODGE,
            PLAYER_ACTION::SKILL_A,
            PLAYER_ACTION::SKILL_B,
            PLAYER_ACTION::SKILL_C,
            PLAYER_ACTION::SKILL_D,
            PLAYER_ACTION::COUNTER_ATTACK
        };

        for (const PLAYER_ACTION action_ : actions_)
        {
            const bool selected_ = debugSelectedAction == action_;
            if (ImGui::Selectable(GetPlayerActionName(action_), selected_))
            {
                debugSelectedAction = action_;
            }

            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Force Change Action"))
    {
        ForceChangeAction(debugSelectedAction);
    }

    ImGui::SameLine();

    if (ImGui::Button("Notify Action End"))
    {
        NotifyActionEnd();
    }

    ImGui::SeparatorText("Runtime Command");

    if (ImGui::Button("Cancel On"))
    {
        SetCanCancel(true);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel Off"))
    {
        SetCanCancel(false);
    }

    ImGui::SeparatorText("Params");

    ImGui::TextUnformatted("Input: KeyConfig Player.* actions / axes");
    ImGui::DragFloat("Move Speed", &moveSpeed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Guard Move Speed", &guardMoveSpeed, 0.1f, 0.0f, 100.0f);
    if (ImGui::TreeNode("Normal Guard / Effects"))
    {
        ImGui::DragFloat("Guard Release Blend Time", &guardSettings.releaseBlendTime, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Guard Hit Cancel Time", &guardSettings.hitCancelTime, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Guard Break Recovery Time", &guardSettings.breakRecoveryTime, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Guard Reaction Maximum Time", &guardSettings.reactionMaximumTime, 0.05f, 0.05f, 20.0f);
        ImGui::InputText("Guard Hit Animation", &guardSettings.hitAnimation);
        ImGui::InputText("Guard Break Animation", &guardSettings.breakAnimation);
        ImGui::InputText("Shield Object Name", &guardSettings.shieldObjectName);
        ImGui::InputText("Guard MeshVFX Asset", &guardSettings.meshVFXAsset);
        const auto owner_ = GetGameObject();
        if (owner_ && owner_->manager && ImGui::BeginCombo("Select Guard MeshVFX", guardSettings.meshVFXAsset.c_str()))
        {
            for (const auto& path_ : owner_->GetManager().GetEffectManager().GetMeshVFXAssetPaths())
                if (ImGui::Selectable(path_.c_str(), guardSettings.meshVFXAsset == path_)) guardSettings.meshVFXAsset = path_;
            ImGui::EndCombo();
        }
        ImGui::DragFloat3("Guard MeshVFX Offset", &guardSettings.meshVFXOffset.x, 0.01f);
        ImGui::DragFloat3("Guard MeshVFX Rotation", &guardSettings.meshVFXRotation.x, 0.5f);
        ImGui::DragFloat3("Guard MeshVFX Scale", &guardSettings.meshVFXScale.x, 0.01f, 0.01f, 100.0f);
        ImGui::InputText("Guard Spark Particle Asset", &guardSettings.sparkParticleAsset);
        ImGui::DragFloat("Guard Spark Life Time", &guardSettings.sparkLifeTime, 0.01f, 0.01f, 5.0f);
        ImGui::Text("Reaction: %s / Break: %s / Rearm required: %s", guardReactionActive ? "ON" : "OFF", guardBroken ? "ON" : "OFF", guardRearmRequired ? "YES" : "NO");
        ImGui::TreePop();
    }

    ImGui::DragFloat("Just Dodge Success Invincibility", &justDodgeInvincibilityDuration, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Jump Impulse", &jumpImpulse, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Double Jump Impulse", &doubleJumpImpulse, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Air Control Rate", &airControlRate, 0.01f, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat("Air Control Accel", &airControlAcceleration, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::DragFloat("Air Deceleration", &airDeceleration, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::DragFloat("Strong Hold Threshold", &strongAttackHoldThreshold, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Strong Charge Speed", &strongChargeAttackSpeed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Run Start Duration", &runStartDuration, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Run End Duration", &runEndDuration, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Jump End Duration", &jumpEndDuration, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Jump Impulse Fallback", &jumpImpulseFallbackDuration, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Timeline Action Fallback", &timelineActionFallbackDuration, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Ground Dodge Cooldown", &groundDodgeCooldownDuration, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::Checkbox("Just Success Presentation", &justSuccessPresentationEnabled);
    ImGui::DragFloat("Just Success Time Scale", &justSuccessTimeScale, 0.01f, 0.05f, 1.0f);
    ImGui::DragFloat("Just Success Hold", &justSuccessHoldDuration, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Just Success Return", &justSuccessReturnDuration, 0.01f, 0.01f, 2.0f);
    ImGui::DragFloat("Just Success Muffle Cutoff", &justSuccessMuffleCutoff, 10.0f, 100.0f, 22000.0f);
    ImGui::DragFloat("Counter Tag Duration", &counterTagDuration, 0.01f, 0.0f, 10.0f, "%.2f");
    if (ImGui::DragFloat("Base Armor", &armor, 0.1f, 0.0f, 99999.0f))
    {
        if (!runtimeArmorOverrideActive)
        {
            runtimeArmor = armor;
        }
        if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
        {
            if (const std::shared_ptr<HitReactionComponent> hitReaction_ =
                owner_->GetComponent<HitReactionComponent>())
            {
                hitReaction_->SetArmor(GetCurrentArmor());
                hitReaction_->SetReactionImpactThresholds(
                    hitMediumImpactThreshold,
                    hitHeavyImpactThreshold,
                    airHitKnockdownImpactThreshold);
            }
        }
    }
    ImGui::Text("Current Armor: %.1f%s", GetCurrentArmor(), runtimeArmorOverrideActive ? " (Runtime)" : " (Base)");
    if (ImGui::Button("Armor 10"))
    {
        SetRuntimeArmor(10.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Armor"))
    {
        ResetRuntimeArmor();
    }
    if (ImGui::DragFloat("Hit Medium Impact", &hitMediumImpactThreshold, 0.1f, 0.0f, 99999.0f) ||
        ImGui::DragFloat("Hit Heavy Impact", &hitHeavyImpactThreshold, 0.1f, 0.0f, 99999.0f) ||
        ImGui::DragFloat("Air Hit Knockdown Impact", &airHitKnockdownImpactThreshold, 0.1f, 0.0f, 99999.0f))
    {
        if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
        {
            if (const std::shared_ptr<HitReactionComponent> hitReaction_ =
                owner_->GetComponent<HitReactionComponent>())
            {
                hitReaction_->SetReactionImpactThresholds(
                    hitMediumImpactThreshold,
                    hitHeavyImpactThreshold,
                    airHitKnockdownImpactThreshold);
            }
        }
    }

    ImGui::SeparatorText("Attack Facing Assist");
    ImGui::Checkbox("Enabled##AttackFacingAssist", &attackAssistSettings.enabled);
    ImGui::SliderFloat("Assist Strength", &attackAssistSettings.strength, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Assist Turn Speed", &attackAssistSettings.turnSpeed, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::DragFloat("Assist Search Distance", &attackAssistSettings.searchDistance, 0.1f, 0.0f, 100.0f, "%.2f");
    ImGui::SliderFloat("Camera Center Angle", &attackAssistSettings.cameraCenterAngle, 0.0f, 180.0f, "%.1f deg");
    ImGui::Checkbox("Lock Target On Enter", &attackAssistSettings.lockTargetOnAttackEnter);
    ImGui::Checkbox("Allow Retarget During Attack", &attackAssistSettings.allowRetargetDuringAttack);
    ImGui::Checkbox("Nearest Fallback When Non Lock-On", &attackAssistSettings.nonLockOnUseNearestFallback);
    ImGui::SliderFloat("Non Lock-On Max Angle", &attackAssistSettings.nonLockOnMaximumTargetAngleDegrees, 0.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Lock-On Max Angle", &attackAssistSettings.lockOnMaximumTargetAngleDegrees, 0.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Max Total Rotation", &attackAssistSettings.maximumTotalRotationDegrees, 0.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Facing Start Time", &attackAssistSettings.facingAssistStartNormalizedTime, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Facing Full End Time", &attackAssistSettings.facingAssistFullStrengthEndNormalizedTime, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Facing End Time", &attackAssistSettings.facingAssistEndNormalizedTime, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Minimum Input Magnitude", &attackAssistSettings.minimumInputMagnitude, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Prefer Move Input Direction", &attackAssistSettings.preferMoveInputDirection);
    ImGui::Checkbox("Prefer Camera Forward Direction", &attackAssistSettings.preferCameraForwardDirection);
    ImGui::Checkbox("Preserve Target Across Combo", &attackAssistSettings.preserveTargetAcrossCombo);
    ImGui::Checkbox("Stop Assist When Target Invalid", &attackAssistSettings.stopAssistWhenTargetInvalid);

    ImGui::SeparatorText("Attack Position Assist");
    ImGui::SliderFloat("Position Strength", &attackAssistSettings.positionStrength, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Position Speed", &attackAssistSettings.positionSpeed, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::DragFloat("Max Position Correction", &attackAssistSettings.maxPositionCorrection, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Target Stop Distance", &attackAssistSettings.targetStopDistance, 0.01f, 0.0f, 10.0f, "%.2f");

    ImGui::TextDisabled("Priority: Lock-On > Move Input > Camera Forward > Player Forward");
}

// Blackboardに値やCategoryを最初に登録する機構。
void PlayerControllerComponent::InitializeBlackboardValues()
{
    // PlayerとAnimationの両方から読む初期キーをそろえ、未設定キーによる遷移漏れを防ぐ。
    if (!blackboard)
    {
        auto owner_ = GetGameObject();
        blackboard = owner_ ? owner_->GetOrAddComponent<BlackboardComponent>() : nullptr;
    }

    if (!blackboard) return;
    blackboard->SetBool("Anim", "EnemyExecutionActive", false);
    blackboard->SetBool("Anim", "DeathSequenceActive", false);

    blackboard->SetString(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CURRENT_SKILL, "");
    blackboard->SetInt(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::SKILL_PHASE, -1);
    blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::IS_SKILL_ACTIVE, false);

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard->SetInt(category_, PlayerBlackboardKey::ACTION, static_cast<int>(PLAYER_ACTION::NONE));
        blackboard->SetInt(category_, PlayerBlackboardKey::ACTION_GROUP, static_cast<int>(PLAYER_ACTION_GROUP::NONE));
        blackboard->SetInt(category_, PlayerBlackboardKey::ATTACK_TYPE, static_cast<int>(PLAYER_ATTACK_TYPE::NONE));
        blackboard->SetInt(category_, PlayerBlackboardKey::COMBO_INDEX, 0);

        blackboard->SetVector2(category_, PlayerBlackboardKey::MOVE_INPUT, { 0.0f, 0.0f });
        blackboard->SetVector2(category_, PlayerBlackboardKey::GUARD_MOVE_INPUT, { 0.0f, 0.0f });
        blackboard->SetVector2(category_, PlayerBlackboardKey::DODGE_INPUT, { 0.0f, 0.0f });
        blackboard->SetVector2(category_, PlayerBlackboardKey::HIT_DIRECTION, { 0.0f, 0.0f });
        blackboard->SetBool(category_, PlayerBlackboardKey::GUARD_INPUT, false);
        blackboard->SetFloat(category_, PlayerBlackboardKey::HIT_IMPACT_POWER, 0.0f);
        blackboard->SetFloat(category_, PlayerBlackboardKey::HIT_ARMOR, GetCurrentArmor());
        blackboard->SetInt(category_, PlayerBlackboardKey::HIT_REACTION_LEVEL, 0);
        blackboard->SetInt(category_, PlayerBlackboardKey::LOCOMOTION_PHASE, static_cast<int>(PLAYER_LOCOMOTION_PHASE::IDLE));

        blackboard->SetBool(category_, PlayerBlackboardKey::HIT_TRIGGER, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_HIT_LIGHT, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_HIT_MEDIUM, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_HIT_HEAVY, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_HIT_REACTION, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_HIT_RECOVERABLE, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_HIT_KNOCKDOWN, false);

        blackboard->SetBool(category_, PlayerBlackboardKey::IS_IDLE, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_MOVE, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_LOCOMOTION, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_ATTACK, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_GUARD, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUST_GUARD, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_DODGE, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_ACTION, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_ATTACK, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_WEAK_ATTACK, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_STRONG_ATTACK, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_STRONG_CHARGE_ATTACK, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_DODGE, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP_START, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP_DOUBLE, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP_LOOP, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP_END, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_GROUNDED, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_RUN_START, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_RUN_LOOP, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_RUN_END, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_SKILL, false);

        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_COUNTER, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::COUNTER_TAG, false);
        blackboard->SetString(category_, PlayerBlackboardKey::COUNTER_TAG_SOURCE, "");
        blackboard->SetBool(category_, PlayerBlackboardKey::DODGED_COUNTER_TAG, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::GUARDED_COUNTER_TAG, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::JUST_GUARD_COUNTER_TAG, false);

        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_CANCEL, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_DODGE_CANCEL, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_ACTION_BRANCH, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_BUFFER_INPUT, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_COMBO, false);
        blackboard->SetBool(category_, PlayerBlackboardKey::ACTION_ENDED, false);
    }

    blackboard->SetBool(
        PlayerBlackboardCategory::ANIM,
        PlayerBlackboardKey::ATTACK_ENTER,
        false);
}

void PlayerControllerComponent::Serialize(nlohmann::json& j_) const
{
    // Player調整値とキャンセルルールをScene JSONへ保存する。
    j_["SkillDefinitionsPath"] = skillDefinitionsPath;
    skillLoadout.Serialize(j_["SkillLoadout"]);
    j_["MoveSpeed"] = moveSpeed;
    j_["GuardMoveSpeed"] = guardMoveSpeed;
    j_["GuardSettings"] = {
        { "GuardReleaseBlendTime", guardSettings.releaseBlendTime },
        { "GuardHitCancelTime", guardSettings.hitCancelTime },
        { "GuardBreakRecoveryTime", guardSettings.breakRecoveryTime },
        { "GuardReactionMaximumTime", guardSettings.reactionMaximumTime },
        { "GuardHitAnimation", guardSettings.hitAnimation },
        { "GuardBreakAnimation", guardSettings.breakAnimation },
        { "GuardMeshVFXAsset", guardSettings.meshVFXAsset },
        { "ShieldObjectName", guardSettings.shieldObjectName },
        { "GuardMeshVFXOffset", { guardSettings.meshVFXOffset.x, guardSettings.meshVFXOffset.y, guardSettings.meshVFXOffset.z } },
        { "GuardMeshVFXRotation", { guardSettings.meshVFXRotation.x, guardSettings.meshVFXRotation.y, guardSettings.meshVFXRotation.z } },
        { "GuardMeshVFXScale", { guardSettings.meshVFXScale.x, guardSettings.meshVFXScale.y, guardSettings.meshVFXScale.z } },
        { "GuardSparkParticleAsset", guardSettings.sparkParticleAsset },
        { "GuardSparkLifeTime", guardSettings.sparkLifeTime }
    };

    j_["JustDodgeInvincibilityDuration"] = justDodgeInvincibilityDuration;
    j_["JumpImpulse"] = jumpImpulse;
    j_["DoubleJumpImpulse"] = doubleJumpImpulse;
    j_["AirControlRate"] = airControlRate;
    j_["AirControlAcceleration"] = airControlAcceleration;
    j_["AirDeceleration"] = airDeceleration;
    j_["StrongAttackHoldThreshold"] = strongAttackHoldThreshold;
    j_["StrongChargeAttackSpeed"] = strongChargeAttackSpeed;
    j_["RunStartDuration"] = runStartDuration;
    j_["RunEndDuration"] = runEndDuration;
    j_["JumpEndDuration"] = jumpEndDuration;
    j_["JumpImpulseFallbackDuration"] = jumpImpulseFallbackDuration;
    j_["TimelineActionFallbackDuration"] = timelineActionFallbackDuration;
    j_["GroundDodgeCooldownDuration"] = groundDodgeCooldownDuration;
    j_["JustSuccessPresentationEnabled"] = justSuccessPresentationEnabled;
    j_["JustSuccessTimeScale"] = justSuccessTimeScale;
    j_["JustSuccessHoldDuration"] = justSuccessHoldDuration;
    j_["JustSuccessReturnDuration"] = justSuccessReturnDuration;
    j_["JustSuccessMuffleCutoff"] = justSuccessMuffleCutoff;
    j_["CounterTagDuration"] = counterTagDuration;
    j_["Armor"] = armor;
    j_["HitMediumImpactThreshold"] = hitMediumImpactThreshold;
    j_["HitHeavyImpactThreshold"] = hitHeavyImpactThreshold;
    j_["AirHitKnockdownImpactThreshold"] = airHitKnockdownImpactThreshold;
    j_["AttackAssist"] =
    {
        { "Enabled", attackAssistSettings.enabled },
        { "Strength", attackAssistSettings.strength },
        { "TurnSpeed", attackAssistSettings.turnSpeed },
        { "SearchDistance", attackAssistSettings.searchDistance },
        { "CameraCenterAngle", attackAssistSettings.cameraCenterAngle },
        { "LockTargetOnAttackEnter", attackAssistSettings.lockTargetOnAttackEnter },
        { "AllowRetargetDuringAttack", attackAssistSettings.allowRetargetDuringAttack },
        { "NonLockOnUseNearestFallback", attackAssistSettings.nonLockOnUseNearestFallback },
        { "NonLockOnMaximumTargetAngleDegrees", attackAssistSettings.nonLockOnMaximumTargetAngleDegrees },
        { "LockOnMaximumTargetAngleDegrees", attackAssistSettings.lockOnMaximumTargetAngleDegrees },
        { "MaximumTotalRotationDegrees", attackAssistSettings.maximumTotalRotationDegrees },
        { "FacingAssistStartNormalizedTime", attackAssistSettings.facingAssistStartNormalizedTime },
        { "FacingAssistFullStrengthEndNormalizedTime", attackAssistSettings.facingAssistFullStrengthEndNormalizedTime },
        { "FacingAssistEndNormalizedTime", attackAssistSettings.facingAssistEndNormalizedTime },
        { "MinimumInputMagnitude", attackAssistSettings.minimumInputMagnitude },
        { "PreferMoveInputDirection", attackAssistSettings.preferMoveInputDirection },
        { "PreferCameraForwardDirection", attackAssistSettings.preferCameraForwardDirection },
        { "PreserveTargetAcrossCombo", attackAssistSettings.preserveTargetAcrossCombo },
        { "StopAssistWhenTargetInvalid", attackAssistSettings.stopAssistWhenTargetInvalid },
        { "PositionStrength", attackAssistSettings.positionStrength },
        { "PositionSpeed", attackAssistSettings.positionSpeed },
        { "MaxPositionCorrection", attackAssistSettings.maxPositionCorrection },
        { "TargetStopDistance", attackAssistSettings.targetStopDistance }
    };
    auto writeTransitions_ = [](const std::vector<PlayerCancelTransition>& transitions_)
    {
        nlohmann::json transitionsJson_ = nlohmann::json::array();
        for (const PlayerCancelTransition& transition_ : transitions_)
        {
            transitionsJson_.push_back(
                {
                    { "From", static_cast<int>(transition_.from) },
                    { "To", static_cast<int>(transition_.to) }
                });
        }
        return transitionsJson_;
    };

    j_["DodgeCancelTransitions"] = writeTransitions_(dodgeCancelTransitions);
    j_["ActionBranchTransitions"] = writeTransitions_(actionBranchTransitions);
}

void PlayerControllerComponent::Deserialize(const nlohmann::json& j_)
{
    CancelEnemyExecution(false);
    // Scene JSONからPlayer調整値を復元し、範囲外の値は安全な範囲へ丸める。
    skillDefinitionsPath = j_.value("SkillDefinitionsPath", skillDefinitionsPath);
    skillLoadout = PlayerSkillLoadout();
    hasExplicitSkillLoadout = j_.contains("SkillLoadout") && skillLoadout.Deserialize(j_.at("SkillLoadout"));
    moveSpeed = (std::max)(j_.value("MoveSpeed", moveSpeed), 0.0f);
    guardMoveSpeed = (std::max)(j_.value("GuardMoveSpeed", guardMoveSpeed), 0.0f);
    StopNormalGuard();
    guardRearmRequired = false;
    const auto guardJson_ = j_.value("GuardSettings", nlohmann::json::object());
    // 旧Sceneに項目がなくても既定設定で読み、実行中の反応やPool参照は復元しない。
    guardSettings.releaseBlendTime = (std::max)(0.0f, guardJson_.value("GuardReleaseBlendTime", 0.08f));
    guardSettings.hitCancelTime = (std::max)(0.0f, guardJson_.value("GuardHitCancelTime", 0.04f));
    guardSettings.breakRecoveryTime = (std::max)(0.0f, guardJson_.value("GuardBreakRecoveryTime", 0.65f));
    guardSettings.reactionMaximumTime = (std::max)(0.05f, guardJson_.value("GuardReactionMaximumTime", 2.5f));
    guardSettings.hitAnimation = guardJson_.value("GuardHitAnimation", std::string("Block_Hit_Seq"));
    guardSettings.breakAnimation = guardJson_.value("GuardBreakAnimation", std::string("Block_Hit_Break_Seq"));
    guardSettings.meshVFXAsset = guardJson_.value("GuardMeshVFXAsset", std::string("Data/VFX/PlayerGuardShieldEnergy.json"));
    guardSettings.shieldObjectName = guardJson_.value("ShieldObjectName", std::string("Shield"));
    guardSettings.sparkParticleAsset = guardJson_.value("GuardSparkParticleAsset", std::string("Data/Particle/PlayerGuardSparks.json"));
    guardSettings.sparkLifeTime = (std::max)(0.01f, guardJson_.value("GuardSparkLifeTime", 0.35f));
    const auto readVector_ = [&guardJson_](const char* key_, const DirectX::XMFLOAT3& fallback_)
    {
        const auto item_ = guardJson_.find(key_);
        if (item_ == guardJson_.end() || !item_->is_array() || item_->size() != 3) return fallback_;
        return DirectX::XMFLOAT3{ (*item_)[0].get<float>(), (*item_)[1].get<float>(), (*item_)[2].get<float>() };
    };
    guardSettings.meshVFXOffset = readVector_("GuardMeshVFXOffset", { 0.0f, 0.0f, 0.0f });
    guardSettings.meshVFXRotation = readVector_("GuardMeshVFXRotation", { 0.0f, 0.0f, 0.0f });
    guardSettings.meshVFXScale = readVector_("GuardMeshVFXScale", { 1.03f, 1.03f, 1.03f });

    justDodgeInvincibilityDuration = (std::max)(j_.value("JustDodgeInvincibilityDuration", 0.5f), 0.0f);
    jumpImpulse = (std::max)(j_.value("JumpImpulse", jumpImpulse), 0.0f);
    doubleJumpImpulse = (std::max)(j_.value("DoubleJumpImpulse", doubleJumpImpulse), 0.0f);
    airControlRate = std::clamp(j_.value("AirControlRate", airControlRate), 0.0f, 2.0f);
    airControlAcceleration = (std::max)(j_.value("AirControlAcceleration", airControlAcceleration), 0.0f);
    airDeceleration = (std::max)(j_.value("AirDeceleration", airDeceleration), 0.0f);
    strongAttackHoldThreshold = (std::max)(j_.value("StrongAttackHoldThreshold", strongAttackHoldThreshold), 0.0f);
    strongChargeAttackSpeed = (std::max)(j_.value("StrongChargeAttackSpeed", strongChargeAttackSpeed), 0.0f);
    runStartDuration = (std::max)(j_.value("RunStartDuration", runStartDuration), 0.0f);
    runEndDuration = (std::max)(j_.value("RunEndDuration", runEndDuration), 0.0f);
    jumpEndDuration = (std::max)(j_.value("JumpEndDuration", jumpEndDuration), 0.0f);
    jumpImpulseFallbackDuration = (std::max)(
        j_.value("JumpImpulseFallbackDuration", jumpImpulseFallbackDuration),
        0.0f);
    timelineActionFallbackDuration = (std::max)(
        j_.value("TimelineActionFallbackDuration", timelineActionFallbackDuration),
        0.0f);
    groundDodgeCooldownDuration = (std::max)(
        j_.value("GroundDodgeCooldownDuration", groundDodgeCooldownDuration),
        0.0f);
    justSuccessPresentationEnabled = j_.value("JustSuccessPresentationEnabled", true);
    justSuccessTimeScale = std::clamp(j_.value("JustSuccessTimeScale", 0.25f), 0.05f, 1.0f);
    justSuccessHoldDuration = std::clamp(j_.value("JustSuccessHoldDuration", 0.25f), 0.0f, 2.0f);
    justSuccessReturnDuration = std::clamp(j_.value("JustSuccessReturnDuration", 0.2f), 0.01f, 2.0f);
    justSuccessMuffleCutoff = std::clamp(j_.value("JustSuccessMuffleCutoff", 1200.0f), 100.0f, 22000.0f);
    counterTagDuration = (std::max)(
        j_.value("CounterTagDuration", counterTagDuration),
        0.0f);
    armor = (std::max)(j_.value("Armor", armor), 0.0f);
    if (!runtimeArmorOverrideActive)
    {
        runtimeArmor = armor;
    }
    hitMediumImpactThreshold = (std::max)(
        j_.value("HitMediumImpactThreshold", hitMediumImpactThreshold),
        0.0f);
    hitHeavyImpactThreshold = (std::max)(
        j_.value("HitHeavyImpactThreshold", hitHeavyImpactThreshold),
        0.0f);
    airHitKnockdownImpactThreshold = (std::max)(
        j_.value("AirHitKnockdownImpactThreshold", airHitKnockdownImpactThreshold),
        0.0f);
    if (j_.contains("AttackAssist") && j_["AttackAssist"].is_object())
    {
        const auto& attackAssistJson_ = j_["AttackAssist"];
        attackAssistSettings.enabled = attackAssistJson_.value("Enabled", attackAssistSettings.enabled);
        attackAssistSettings.strength = std::clamp(
            attackAssistJson_.value("Strength", attackAssistSettings.strength),
            0.0f,
            1.0f);
        attackAssistSettings.turnSpeed = (std::max)(
            attackAssistJson_.value("TurnSpeed", attackAssistSettings.turnSpeed),
            0.0f);
        attackAssistSettings.searchDistance = (std::max)(
            attackAssistJson_.value("SearchDistance", attackAssistSettings.searchDistance),
            0.0f);
        attackAssistSettings.cameraCenterAngle = std::clamp(
            attackAssistJson_.value("CameraCenterAngle", attackAssistSettings.cameraCenterAngle),
            0.0f,
            180.0f);
        attackAssistSettings.lockTargetOnAttackEnter =
            attackAssistJson_.value("LockTargetOnAttackEnter", attackAssistSettings.lockTargetOnAttackEnter);
        attackAssistSettings.allowRetargetDuringAttack =
            attackAssistJson_.value("AllowRetargetDuringAttack", attackAssistSettings.allowRetargetDuringAttack);
        attackAssistSettings.nonLockOnUseNearestFallback =
            attackAssistJson_.value("NonLockOnUseNearestFallback", attackAssistSettings.nonLockOnUseNearestFallback);
        attackAssistSettings.nonLockOnMaximumTargetAngleDegrees = std::clamp(
            attackAssistJson_.value("NonLockOnMaximumTargetAngleDegrees", attackAssistSettings.nonLockOnMaximumTargetAngleDegrees),
            0.0f,
            180.0f);
        attackAssistSettings.lockOnMaximumTargetAngleDegrees = std::clamp(
            attackAssistJson_.value("LockOnMaximumTargetAngleDegrees", attackAssistSettings.lockOnMaximumTargetAngleDegrees),
            0.0f,
            180.0f);
        attackAssistSettings.maximumTotalRotationDegrees = std::clamp(
            attackAssistJson_.value("MaximumTotalRotationDegrees", attackAssistSettings.maximumTotalRotationDegrees),
            0.0f,
            180.0f);
        attackAssistSettings.facingAssistStartNormalizedTime = std::clamp(
            attackAssistJson_.value("FacingAssistStartNormalizedTime", attackAssistSettings.facingAssistStartNormalizedTime),
            0.0f,
            1.0f);
        attackAssistSettings.facingAssistFullStrengthEndNormalizedTime = std::clamp(
            attackAssistJson_.value("FacingAssistFullStrengthEndNormalizedTime", attackAssistSettings.facingAssistFullStrengthEndNormalizedTime),
            0.0f,
            1.0f);
        attackAssistSettings.facingAssistEndNormalizedTime = std::clamp(
            attackAssistJson_.value("FacingAssistEndNormalizedTime", attackAssistSettings.facingAssistEndNormalizedTime),
            0.0f,
            1.0f);
        attackAssistSettings.minimumInputMagnitude = std::clamp(
            attackAssistJson_.value("MinimumInputMagnitude", attackAssistSettings.minimumInputMagnitude),
            0.0f,
            1.0f);
        attackAssistSettings.preferMoveInputDirection =
            attackAssistJson_.value("PreferMoveInputDirection", attackAssistSettings.preferMoveInputDirection);
        attackAssistSettings.preferCameraForwardDirection =
            attackAssistJson_.value("PreferCameraForwardDirection", attackAssistSettings.preferCameraForwardDirection);
        attackAssistSettings.preserveTargetAcrossCombo =
            attackAssistJson_.value("PreserveTargetAcrossCombo", attackAssistSettings.preserveTargetAcrossCombo);
        attackAssistSettings.stopAssistWhenTargetInvalid =
            attackAssistJson_.value("StopAssistWhenTargetInvalid", attackAssistSettings.stopAssistWhenTargetInvalid);

        attackAssistSettings.positionStrength = std::clamp(
            attackAssistJson_.value("PositionStrength", attackAssistSettings.positionStrength),
            0.0f,
            1.0f);

        attackAssistSettings.positionSpeed = (std::max)(
            attackAssistJson_.value("PositionSpeed", attackAssistSettings.positionSpeed),
            0.0f);

        attackAssistSettings.maxPositionCorrection = (std::max)(
            attackAssistJson_.value("MaxPositionCorrection", attackAssistSettings.maxPositionCorrection),
            0.0f);

        attackAssistSettings.targetStopDistance = (std::max)(
            attackAssistJson_.value("TargetStopDistance", attackAssistSettings.targetStopDistance),
            0.0f);
    }
    auto readTransitions_ = [this](const nlohmann::json& transitionsJson_)
    {
        // 保存値をActionへ変換する前に、有効範囲と未選択値を検査する。
        for (const auto& transitionJson_ : transitionsJson_)
        {
            const int from_ = transitionJson_.value("From", static_cast<int>(PLAYER_ACTION::NONE));
            const int to_ = transitionJson_.value("To", static_cast<int>(PLAYER_ACTION::NONE));
            if (from_ < 0 || from_ > static_cast<int>(PLAYER_ACTION::SKILL_D) || from_ == static_cast<int>(PLAYER_ACTION::NONE) ||
                to_ < 0 || to_ > static_cast<int>(PLAYER_ACTION::SKILL_D) || to_ == static_cast<int>(PLAYER_ACTION::NONE))
            {
                continue;
            }
            AddCancelTransition(
                static_cast<PLAYER_ACTION>(from_),
                static_cast<PLAYER_ACTION>(to_));
        }
    };

    if (j_.contains("DodgeCancelTransitions") && j_["DodgeCancelTransitions"].is_array())
    {
        dodgeCancelTransitions.clear();
        readTransitions_(j_["DodgeCancelTransitions"]);
    }

    if (j_.contains("ActionBranchTransitions") && j_["ActionBranchTransitions"].is_array())
    {
        actionBranchTransitions.clear();
        readTransitions_(j_["ActionBranchTransitions"]);
    }

    if (j_.contains("CancelTransitions") && j_["CancelTransitions"].is_array())
    {
        // 旧形式の共通リストも読み込み、既存の分類処理を通して遷移ルールへ追加する。
        readTransitions_(j_["CancelTransitions"]);
    }

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        if (const std::shared_ptr<HitReactionComponent> hitReaction_ =
            owner_->GetComponent<HitReactionComponent>())
        {
            hitReaction_->SetArmor(GetCurrentArmor());
            hitReaction_->SetReactionImpactThresholds(
                hitMediumImpactThreshold,
                hitHeavyImpactThreshold,
                airHitKnockdownImpactThreshold);
        }
    }

    EnsureDefaultCancelTransitions();
}

REGISTER_COMPONENT(PlayerControllerComponent, "PlayerController")

// 実行条件をここへ集約し、入力ResolverやStateへ消費条件を分散させない。
bool PlayerControllerComponent::CanExecuteSkill(PLAYER_ACTION action_) const
{
    const auto* definition_ = GetSkillDefinition(action_);
    std::string error_;
    if (!definition_ || !IsSkillCooldownReady(action_)) return false;
    if (IsAirborneForAction() ? !definition_->canUseInAir : !definition_->canUseOnGround) return false;
    return SkillExecution::CanExecute(gameObject.lock(), *definition_, error_);
}

// RequestActionの受付条件を副作用なしで表示へ渡し、先行入力やコンボ予約を変更しない。
bool PlayerControllerComponent::CanDisplayActionReady(PLAYER_ACTION action_) const
{
    if (enemyExecutionCaptured || enemyExecutionRecovering || guardBroken || action_ == PLAYER_ACTION::NONE) return false;
    if (action_ == PLAYER_ACTION::GUARD && guardRearmRequired) return false;
    if (action_ == PLAYER_ACTION::JUMP && !IsGrounded()) return false;
    if (IsAirAction(action_) && !IsAirborneForAction()) return false;
    if (IsDodgeAction(action_) && !CanStartDodgeAction(action_)) return false;
    if (PlayerSkillLoadout::GetSlotIndex(action_) >= 0)
    {
        if (!CanDisplaySkillReady(action_)) return false;
    }
    else if (IsAirborneForAction() && IsGroundOnlyAction(action_)) return false;

    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (currentAction_ == action_)
        return (action_ == PLAYER_ACTION::WEAK_ATTACK || action_ == PLAYER_ACTION::AIR_WEAK_ATTACK) &&
            (actionRuntime.canCombo || actionRuntime.canBufferInput);
    if (currentAction_ == PLAYER_ACTION::NONE || GetPlayerActionGroup(currentAction_) == PLAYER_ACTION_GROUP::LOCOMOTION) return true;
    // 実際の即時派生とHUDの受付表示を一致させる。
    if (currentAction_ == PLAYER_ACTION::GUARD && action_ == PLAYER_ACTION::JUST_GUARD) return true;
    return IsDodgeCancelAction(action_)
        ? actionRuntime.canDodgeCancel && HasDodgeCancelTransition(currentAction_, action_)
        : actionRuntime.canActionBranch && HasActionBranchTransition(currentAction_, action_);
}

// 表示の問い合わせから毎フレーム同じ素材を検証せず、実際の発動時の検証は従来どおり行う。
bool PlayerControllerComponent::CanDisplaySkillReady(PLAYER_ACTION action_) const
{
    const int slotIndex_ = PlayerSkillLoadout::GetSlotIndex(action_);
    const auto* definition_ = GetSkillDefinition(action_);
    if (slotIndex_ < 0 || !definition_ || !definition_->enabled) return false;
    if (GetRemainingSkillCooldown(definition_->name) > 0.0f) return false;
    if (IsAirborneForAction() ? !definition_->canUseInAir : !definition_->canUseOnGround) return false;

    auto& cache_ = skillHudAssetCache[static_cast<std::size_t>(slotIndex_)];
    if (cache_.skillName != definition_->name || cache_.recheckTime <= hudObservationTime)
    {
        BS_PROFILE_SCOPE("PlayerControllerComponent::ValidateHudSkillAssets");
        constexpr float HUD_ASSET_RECHECK_INTERVAL = 0.25f;
        std::string error_;
        cache_.available = SkillExecution::CanExecute(gameObject.lock(), *definition_, error_);
        cache_.skillName = definition_->name;
        cache_.recheckTime = hudObservationTime + HUD_ASSET_RECHECK_INTERVAL;
    }
    return cache_.available;
}

// 保存後の共通ライブラリをPlayer入力表へ反映する。
bool PlayerControllerComponent::ReloadSkillDefinitions(const std::string& path_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::ReloadSkillDefinitions");
    const auto* skillState_ = stateMachine.GetCurrentStateAs<PlayerSkillState>();
    if (skillState_ && skillState_->IsActive()) return false;
    if (!LoadPlayerSkillDefinitions(path_, skillInputs, skillDefinitions)) return false;
    skillDefinitionsPath = path_;
    ApplyLegacySkillLoadout();
    // 定義の再読込では、同名スキルのPhaseや参照素材が変わるため表示用の検証も破棄する。
    skillHudAssetCache = {};
    return true;
}
// Skill Actionに対応する定義を取得する。
const PlayerSkillDefinition* PlayerControllerComponent::GetSkillDefinition(PLAYER_ACTION action_) const
{
    const int slotIndex_ = PlayerSkillLoadout::GetSlotIndex(action_);
    return slotIndex_ >= 0 ? GetSkillDefinitionByName(skillLoadout.GetSkillName(static_cast<std::size_t>(slotIndex_))) : nullptr;
}

// 現在のSkillStateへ中断を通知し、通常行動と同じ終了経路へ接続する。
void PlayerControllerComponent::StopSkill()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::StopSkill");
    if (auto* skillState_ = stateMachine.GetCurrentStateAs<PlayerSkillState>()) skillState_->StopSkill();
}

// Actionの番号ではなく定義名で残り時間を参照し、スロット変更でも同じSkillの待ち時間を維持する。
float PlayerControllerComponent::GetRemainingSkillCooldown(PLAYER_ACTION action_) const
{
    const auto* definition_ = GetSkillDefinition(action_);
    if (!definition_) return 0.0f;
    return GetRemainingSkillCooldown(definition_->name);
}

float PlayerControllerComponent::GetSkillCooldownDuration(PLAYER_ACTION action_) const
{
    const auto* definition_ = GetSkillDefinition(action_);
    return definition_ ? definition_->cooldown : 0.0f;
}

bool PlayerControllerComponent::IsSkillCooldownReady(PLAYER_ACTION action_) const
{
    return GetSkillDefinition(action_) && GetRemainingSkillCooldown(action_) <= 0.0f;
}

// 入力を受け付けた時点ではなく、Stateで開始が成立したスキルの待ち時間を設定する。
void PlayerControllerComponent::NotifySkillStarted(PLAYER_ACTION action_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::NotifySkillStarted");
    if (const auto* definition_ = GetSkillDefinition(action_))
        skillCooldowns[definition_->name] = definition_->cooldown;
}

// Playerへ渡されたTimeScale適用済みDeltaTimeを使用し、独自の時刻取得は行わない。
void PlayerControllerComponent::UpdateSkillCooldowns(float deltaTime_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::UpdateSkillCooldowns");
    const float elapsedTime_ = (std::max)(0.0f, deltaTime_);
    for (auto& cooldown_ : skillCooldowns)
        cooldown_.second = (std::max)(0.0f, cooldown_.second - elapsedTime_);
}

// Scene開始時とデバッグ操作で実行時の待ち時間だけを消す。
void PlayerControllerComponent::ResetSkillCooldowns()
{
    skillCooldowns.clear();
}

// カタログと装備を分け、どの種類のSkillも名前で同じ経路へ渡す。
const PlayerSkillDefinition* PlayerControllerComponent::GetSkillDefinitionByName(const std::string& skillName_) const
{
    for (const auto& definition_ : skillDefinitions)
        if (definition_.name == skillName_) return &definition_;
    return nullptr;
}

float PlayerControllerComponent::GetRemainingSkillCooldown(const std::string& skillName_) const
{
    const auto iterator_ = skillCooldowns.find(skillName_);
    return iterator_ != skillCooldowns.end() ? iterator_->second : 0.0f;
}

// 実行中のスキルと装備定義の食い違いを避けるため、SkillStateが非実行中のときだけ許可する。
bool PlayerControllerComponent::CanEditSkillLoadout() const
{
    const auto* state_ = stateMachine.GetCurrentStateAs<PlayerSkillState>();
    return !state_ || !state_->IsActive();
}

// カタログ内のスキルまたは空欄を装備し、以降は旧形式からの自動装備移行を行わない。
bool PlayerControllerComponent::SetEquippedSkill(std::size_t slotIndex_, const std::string& skillName_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::SetEquippedSkill");
    if (!CanEditSkillLoadout() || slotIndex_ >= PlayerSkillLoadout::SLOT_COUNT) return false;
    if (!skillName_.empty() && !GetSkillDefinitionByName(skillName_)) return false;
    if (!skillLoadout.SetSkillName(slotIndex_, skillName_)) return false;
    hasExplicitSkillLoadout = true;
    return true;
}

const std::string& PlayerControllerComponent::GetEquippedSkillName(std::size_t slotIndex_) const
{
    return skillLoadout.GetSkillName(slotIndex_);
}

// 枠番号を既存のActionへ変換し、直接Stateを変えず共通の受付条件へ渡す。
bool PlayerControllerComponent::RequestSkillSlot(std::size_t slotIndex_)
{
    return RequestAction(PlayerSkillLoadout::GetSlotAction(slotIndex_));
}

// Sceneに装備指定が無い旧データだけを移行し、Editorで変更した装備は再読込でも維持する。
void PlayerControllerComponent::ApplyLegacySkillLoadout()
{
    if (hasExplicitSkillLoadout) return;
    std::array<bool, PlayerSkillLoadout::SLOT_COUNT> assigned_{};
    for (const auto& definition_ : skillDefinitions)
    {
        const int slotIndex_ = PlayerSkillLoadout::GetSlotIndex(definition_.action);
        if (slotIndex_ >= 0 && !assigned_[slotIndex_])
        {
            skillLoadout.SetSkillName(slotIndex_, definition_.name);
            assigned_[slotIndex_] = true;
        }
    }
}

// 実行中は編集を止め、選択後はScene保存で装備だけを永続化する。
void PlayerControllerComponent::DrawSkillLoadoutGui()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::DrawSkillLoadoutGui");
    ImGui::SeparatorText("Player Skill Loadout");
    const bool editable_ = CanEditSkillLoadout();
    if (!editable_) ImGui::TextDisabled("Loadout is locked while a skill is running.");
    ImGui::BeginDisabled(!editable_);
    for (std::size_t slotIndex_ = 0; slotIndex_ < PlayerSkillLoadout::SLOT_COUNT; ++slotIndex_)
    {
        ImGui::PushID(static_cast<int>(slotIndex_));
        const std::string currentName_ = GetEquippedSkillName(slotIndex_);
        const std::string label_ = "SkillSlot" + std::to_string(slotIndex_);
        if (ImGui::BeginCombo(label_.c_str(), currentName_.empty() ? "(Empty)" : currentName_.c_str()))
        {
            if (ImGui::Selectable("(Empty)", currentName_.empty())) SetEquippedSkill(slotIndex_, "");
            for (const auto& definition_ : skillDefinitions)
                if (ImGui::Selectable(definition_.name.c_str(), currentName_ == definition_.name))
                    SetEquippedSkill(slotIndex_, definition_.name);
            ImGui::EndCombo();
        }
        if (const auto* definition_ = GetSkillDefinitionByName(currentName_))
            ImGui::Text("%s / %.2f / %.2f sec", GetSkillCategoryName(definition_->category),
                GetRemainingSkillCooldown(currentName_), definition_->cooldown);
        else if (!currentName_.empty()) ImGui::TextDisabled("Skill is missing from the current library.");
        ImGui::PopID();
    }
    ImGui::EndDisabled();
}

// 子の盾Colliderも含め、通常ガードの受付を入力Stateへ同期する。
// 盾を含む子階層のガード判定を通常ガードとして切り替え、受付状態を公開する。
void PlayerControllerComponent::SetNormalGuardEnabled(bool enabled_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::SetNormalGuardEnabled");
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const auto visit_ = [enabled_](const auto& self_, const std::shared_ptr<GameObject>& object_) -> void
    {
        if (!object_) return;
        for (const auto& guard_ : object_->GetComponents<GuardCollider>())
        {
            guard_->SetJustGuardEnabled(false);
            if (enabled_ && !guard_->IsEnabled()) guard_->Activate();
            else if (!enabled_ && guard_->IsEnabled()) guard_->Deactivate();
        }
        if (object_->transform)
            for (const auto& child_ : object_->transform->children)
                if (child_) self_(self_, child_->gameObject.lock());
    };
    visit_(visit_, owner_);
    normalGuardActive = enabled_;
    if (blackboard) blackboard->SetBool("Guard", "Active", enabled_);
}

// 判定と表示の終了を一か所へまとめ、攻撃・回避・被弾への遷移でも残留を防ぐ。
void PlayerControllerComponent::StopNormalGuard()
{
    if (normalGuardActive) SetNormalGuardEnabled(false);
    StopGuardVisual();
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (guardAnimationControlled && animation_) animation_->SetSkillControlled(false);
    guardAnimationControlled = false;
    guardReactionActive = false;
    guardReactionEndRequested = false;
    guardBroken = false;
    guardReactionTimer = 0.0f;
    if (blackboard)
    {
        blackboard->SetBool("Guard", "ReactionActive", false);
        blackboard->SetBool("Guard", "GuardBroken", false);
        blackboard->SetBool("Guard", "GuardSucceeded", false);
    }
}

// 通常GuardとGuardBreakの成功通知だけを受け、通常HitReactionを経由せず反応する。
void PlayerControllerComponent::HandleGuardImpact(bool broken_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::HandleGuardImpact");
    if (GetCurrentAction() != PLAYER_ACTION::GUARD || !normalGuardActive || guardBroken) return;
    const auto owner_ = GetGameObject();
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    PlayGuardSparks();
    if (health_ && health_->IsDead())
    {
        // 致死のGuardBreakでも、以前の被弾情報で死亡時の反応を選ばない。
        if (blackboard)
        {
            float impact_ = 0.0f;
            Vector3 direction_{ 0.0f, 0.0f, 0.0f };
            blackboard->GetFloat("Guard", "ReceivedImpactPower", impact_);
            blackboard->GetVector3("Guard", "Normal", direction_);
            blackboard->SetFloat("Hit", "ImpactPower", impact_);
            blackboard->SetFloat("Hit", "Armor", GetCurrentArmor());
            blackboard->SetVector2("Hit", "Direction", { direction_.x, direction_.z });
            blackboard->SetBool("Hit", "ImpactReacted", true);
        }
        HandleHitReaction();
        return;
    }
    guardBroken = broken_;
    guardReactionActive = true;
    guardReactionEndRequested = false;
    guardReactionTimer = 0.0f;
    if (broken_)
    {
        // 崩れ中は判定を閉じ、押しっぱなしのガード入力だけで復帰直後に再開させない。
        guardRearmRequired = true;
        SetNormalGuardEnabled(false);
        StopGuardVisual();
        SendGuardInputToBlackboard(false);
        if (blackboard)
        {
            blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::IS_GUARD, false);
            blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::IS_GUARD, false);
        }
    }
    actionRuntime.canDodgeCancel = !broken_ && guardSettings.hitCancelTime <= 0.0f;
    actionRuntime.canActionBranch = actionRuntime.canDodgeCancel;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    SyncCancelGateBlackboard();
    if (blackboard) blackboard->SetBool("Guard", "ReactionActive", true);
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const std::string& state_ = broken_ ? guardSettings.breakAnimation : guardSettings.hitAnimation;
    if (animation_ && (!animation_->IsSkillControlled() || guardAnimationControlled) && animation_->GetStates().count(state_))
    {
        // 反応中だけ通常グラフのGuard遷移を止め、同じフレームの上書きを防ぐ。
        guardAnimationControlled = true;
        animation_->SetSkillControlled(true);
        animation_->ChangeState(state_, guardSettings.releaseBlendTime, true);
    }
}

// 終了通知と再生位置を優先し、素材不足でも設定された上限時間で操作を返す。
void PlayerControllerComponent::UpdateGuardReaction(float deltaTime_)
{
    if (!guardReactionActive || GetCurrentAction() != PLAYER_ACTION::GUARD) return;
    BS_PROFILE_SCOPE("PlayerControllerComponent::UpdateGuardReaction");
    guardReactionTimer += (std::max)(0.0f, deltaTime_);
    if (!guardBroken && guardReactionTimer >= guardSettings.hitCancelTime)
    {
        actionRuntime.canDodgeCancel = true;
        actionRuntime.canActionBranch = true;
        SyncCancelGateBlackboard();
    }
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto state_ = animation_ ? animation_->GetCurrentState() : nullptr;
    const std::string& expected_ = guardBroken ? guardSettings.breakAnimation : guardSettings.hitAnimation;
    const float duration_ = animation_ ? animation_->GetCurrentSampleDuration() : 0.0f;
    const bool ended_ = guardReactionEndRequested || (state_ && state_->name == expected_ &&
        duration_ > 0.0001f && animation_->GetAnimatorTime() >= duration_);
    if ((ended_ && (!guardBroken || guardReactionTimer >= guardSettings.breakRecoveryTime)) ||
        guardReactionTimer >= (std::max)(guardSettings.reactionMaximumTime, guardBroken ? guardSettings.breakRecoveryTime : 0.0f))
        FinishGuardReaction();
}

// 通常の受け止めはGuardへ、GuardBreakは入力を再受付する通常Stateへ戻す。
void PlayerControllerComponent::FinishGuardReaction()
{
    const bool broken_ = guardBroken;
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (guardAnimationControlled && animation_) animation_->SetSkillControlled(false);
    guardAnimationControlled = false;
    guardReactionActive = false;
    guardReactionEndRequested = false;
    if (blackboard) blackboard->SetBool("Guard", "ReactionActive", false);
    if (broken_)
    {
        guardRearmRequired = KeyConfig::Instance().ActionPressed(PlayerInputAction::GUARD);
        guardBroken = false;
        const auto input_ = GetMoveInput();
        stateMachine.ChangeState(input_.x != 0.0f || input_.z != 0.0f ? PLAYER_ACTION::MOVE : PLAYER_ACTION::IDLE);
    }
    else
    {
        actionRuntime.canDodgeCancel = true;
        actionRuntime.canActionBranch = true;
        SyncCancelGateBlackboard();
        if (animation_ && animation_->GetStates().count("Block_Walk_Loop_Blend"))
            animation_->ChangeState("Block_Walk_Loop_Blend", guardSettings.releaseBlendTime);
    }
}

// ガード開始時だけPoolから一つ生成し、既存の盾AttachmentのTransformへ追従させる。
void PlayerControllerComponent::StartGuardVisual()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::StartGuardVisual");
    if (const auto visual_ = guardVisual.lock())
        if (visual_->manager && visual_->GetManager().GetEffectManager().IsEffectOwnedBy(visual_, this)) return;
    guardVisual.reset();
    if (guardSettings.meshVFXAsset.empty()) return;
    const auto owner_ = GetGameObject();
    const auto shield_ = owner_ && owner_->manager ? owner_->GetManager().Find(guardSettings.shieldObjectName) : nullptr;
    if (!owner_ || !shield_ || !shield_->transform || !owner_->transform) return;
    // 同名の別キャラクターの盾へ演出を付けないよう、親階層の所属を確認する。
    auto parent_ = shield_->transform->parent.lock();
    bool belongs_ = false;
    while (parent_)
    {
        if (parent_ == owner_->transform) { belongs_ = true; break; }
        parent_ = parent_->parent.lock();
    }
    if (!belongs_) return;
    if (const auto attachment_ = shield_->GetComponent<ModelAttachmentComponent>()) attachment_->SyncAttachment();
    MeshVFXSpawnRequest request_{};
    request_.assetPath = guardSettings.meshVFXAsset;
    request_.lifetimeOwner = owner_->GetComponent<PlayerControllerComponent>();
    const auto visual_ = owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
    if (!visual_ || !visual_->transform) return;
    visual_->transform->SetParent(shield_->transform);
    visual_->transform->localPosition = guardSettings.meshVFXOffset;
    visual_->transform->localRotation = guardSettings.meshVFXRotation;
    visual_->transform->localScale = guardSettings.meshVFXScale;
    visual_->transform->MarkDirty();
    visual_->UpdateTransform();
    guardVisual = visual_;
}

// 明示終了をManagerへ渡し、ガードの出入りでPoolのインスタンスを増殖させない。
void PlayerControllerComponent::StopGuardVisual()
{
    const auto visual_ = guardVisual.lock();
    guardVisual.reset();
    if (!visual_ || !visual_->manager) return;
    // 設定した盾名が変更されても自分の演出は閉じ、再利用済みの別演出は停止しない。
    visual_->GetManager().GetEffectManager().StopEffect(visual_, this);
}

// 成立した接触点から少量の火花を一度だけ出し、JustGuardの専用演出とは分ける。
void PlayerControllerComponent::PlayGuardSparks()
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::PlayGuardSparks");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !blackboard || guardSettings.sparkParticleAsset.empty()) return;
    ParticleSpawnRequest request_{};
    request_.assetPath = guardSettings.sparkParticleAsset;
    request_.lifeTime = guardSettings.sparkLifeTime;
    // BlackboardのVector3で取得し、Particle用のXMFLOAT3へ成分を渡す。
    DirectX::SimpleMath::Vector3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::SimpleMath::Vector3 normal_{ 0.0f, 1.0f, 0.0f };
    blackboard->GetVector3("Guard", "Position", position_);
    blackboard->GetVector3("Guard", "Normal", normal_);
    request_.position = { position_.x, position_.y, position_.z };
    request_.normal = { normal_.x, normal_.y, normal_.z };
    bool hasContact_ = false;
    blackboard->GetBool("Guard", "HasContact", hasContact_);
    if (!hasContact_)
    {
        const auto shield_ = owner_->GetManager().Find(guardSettings.shieldObjectName);
        if (shield_ && shield_->transform) request_.position = shield_->transform->GetWorldPosition();
        else if (owner_->transform) request_.position = owner_->transform->GetWorldPosition();
    }
    request_.overrideAxis = true;
    request_.axis = request_.normal;
    request_.normalOffset = 0.02f;
    owner_->GetManager().GetEffectManager().SpawnParticle(request_);
}
