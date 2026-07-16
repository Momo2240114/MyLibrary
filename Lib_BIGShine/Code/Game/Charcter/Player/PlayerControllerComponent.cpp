#include "PlayerControllerComponent.h"

#include <cmath>

#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Combat/HitReactionComponent.h"
#include "Engine/GamePlay/Combat/JustDodgeReactionComponent.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Physics/CharacterCollider.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/Physics/JustDodgeCollider.h"

#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "PlayerActionState.h"
#include "PlayerAttackState.h"
#include "PlayerDodgeState.h"
#include "PlayerHitState.h"
#include "PlayerIdleState.h"
#include "PlayerJumpState.h"
#include "PlayerMoveState.h"

namespace
{
    inline constexpr int WEAK_ATTACK_COMBO_COUNT = 4;

    namespace PlayerInputAction
    {
        inline constexpr const char* MOVE_X = "Player.MoveX";
        inline constexpr const char* MOVE_Z = "Player.MoveZ";
        inline constexpr const char* WEAK_ATTACK = "Player.WeakAttack";
        inline constexpr const char* STRONG_ATTACK = "Player.StrongAttack";
        inline constexpr const char* JUMP = "Player.Jump";
        inline constexpr const char* LOCK_ON = "Player.LockOn";
        inline constexpr const char* GUARD = "Player.Guard";
        inline constexpr const char* DODGE = "Player.Dodge";
        inline constexpr const char* SKILL_A = "Player.SkillA";
        inline constexpr const char* SKILL_B = "Player.SkillB";
        inline constexpr const char* DEBUG_GUARD_HIT = "Player.DebugGuardHit";
    }

    DirectX::XMFLOAT3 NormalizeHorizontalDirection(
        const DirectX::XMFLOAT3& direction_,
        const DirectX::XMFLOAT3& fallback_)
    {
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
    KeyConfig& keyConfig_ = KeyConfig::Instance();
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
    auto owner_ = GetGameObject();
    if (!owner_) return;

    EnsurePlayerInputBindings();

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
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::SKILL_A, PLAYER_ACTION::SKILL_A);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::SKILL_B, PLAYER_ACTION::SKILL_B);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::COUNTER_ATTACK, PLAYER_ACTION::COUNTER_ATTACK);
    EnsureDefaultCancelTransitions();

    stateMachine.ChangeState(PLAYER_ACTION::IDLE);
}

void PlayerControllerComponent::Update(float dt_)
{
    lastUpdateDeltaTime = (std::max)(dt_, 0.0f);
    stateMachine.Update(dt_);
    TryDodgeCancelInput();
    UpdateAirborneActionContext();
    UpdateDodgeRestrictions(dt_);
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
    return KeyConfig::Instance().ActionPressedIn(PlayerInputAction::JUMP);
}

bool PlayerControllerComponent::IsCurrentAnimationState(const std::string& stateName_)
{
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
    default:
        return true;
    }
}

void PlayerControllerComponent::UpdateTimelineActionSafety(float deltaTime_)
{
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (!IsPlayerActionTimelineDriven(currentAction_) || actionRuntime.actionEnded)
    {
        timelineActionSafetyTimer = 0.0f;
        return;
    }

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
    airborneActionContextActive = true;
    airborneActionContextLeftGround = !IsGrounded();
}

void PlayerControllerComponent::EndAirborneActionContext()
{
    airborneActionContextActive = false;
    airborneActionContextLeftGround = false;
    jumpLoopResumeRequested = false;
}

bool PlayerControllerComponent::ConsumeJumpLoopResumeRequest()
{
    const bool requested_ = jumpLoopResumeRequested;
    jumpLoopResumeRequested = false;
    return requested_;
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetMoveInput() const
{
    return GetCameraRelativeMoveDirection(gameObject.lock(), GetMoveInputAxis());
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetMoveInputAxis() const
{
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

    // Dodge start needs an immediate camera-relative basis, so use a full interpolation rate.
    owner_->transform->SetForwardYawOnly(
        { direction_.x, 0.0f, direction_.z },
        1.0f,
        1.0f);
}

void PlayerControllerComponent::SetHorizontalVelocity(
    const DirectX::XMFLOAT3& dir_,
    float speed_)
{
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
    pendingFacingDirection = { dir_.x, 0.0f, dir_.z };
    hasPendingFacingDirection = true;
}

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
    if (IsAirborneForAction() && IsGroundOnlyAction(nextAction_))
    {
        return false;
    }

    if (currentAction_ == nextAction_)
    {
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
    const bool nextIsDodge_ = IsDodgeAction(nextAction_);
    const bool canUseCancelTransition_ = nextIsDodge_
        ? (actionRuntime.canDodgeCancel && HasDodgeCancelTransition(currentAction_, nextAction_))
        : (actionRuntime.canActionBranch && HasActionBranchTransition(currentAction_, nextAction_));
    const bool canChangeImmediately_ =
        currentAction_ == PLAYER_ACTION::NONE ||
        isLocomotion_ ||
        canUseCancelTransition_;

    if (!canChangeImmediately_)
    {
        if (actionRuntime.canBufferInput || actionRuntime.canCombo)
        {
            actionRuntime.bufferedAction = nextAction_;
            actionRuntime.bufferedComboIndex = requestedComboIndex_;
        }
        return false;
    }

    pendingComboIndex = requestedComboIndex_;
    stateMachine.ChangeState(nextAction_);
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
    return IsDodgeAction(to_)
        ? AddDodgeCancelTransition(from_, to_)
        : AddActionBranchTransition(from_, to_);
}

bool PlayerControllerComponent::RemoveCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    return IsDodgeAction(to_)
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

    AddActionBranchTransition(PLAYER_ACTION::JUMP, PLAYER_ACTION::IDLE);
    AddActionBranchTransition(PLAYER_ACTION::JUMP, PLAYER_ACTION::MOVE);
}

std::shared_ptr<CameraLockOnComponent>
PlayerControllerComponent::ResolveCameraLockOnComponent() const
{
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

void PlayerControllerComponent::NotifyGuardedAttack()
{
    if (GetCurrentAction() == PLAYER_ACTION::GUARD)
    {
        RequestAction(PLAYER_ACTION::JUST_GUARD);
    }
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
    if (actionRuntime.bufferedAction == PLAYER_ACTION::NONE)
    {
        return;
    }

    if (IsDodgeAction(actionRuntime.bufferedAction) != dodgeGate_)
    {
        return;
    }

    const PLAYER_ACTION nextAction_ = actionRuntime.bufferedAction;
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

    if (actionRuntime.bufferedAction != GetCurrentAction() &&
        IsPlayerActionTimelineDriven(GetCurrentAction()) &&
        !actionRuntime.actionEnded)
    {
        NotifyActionEnd();
        return;
    }

    ConsumeBufferedAction();
}

void PlayerControllerComponent::NotifyComboTransition()
{
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
    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (!IsPlayerActionTimelineDriven(currentAction_) &&
        currentAction_ != PLAYER_ACTION::JUMP)
    {
        return;
    }

    if (IsPlayerActionTimelineDriven(currentAction_))
    {
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
        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
    }

    if (ConsumeBufferedAction())
    {
        return;
    }

    if (IsAirAction(currentAction_))
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

void PlayerControllerComponent::HandleHitReaction()
{
    actionRuntime.canDodgeCancel = false;
    actionRuntime.canActionBranch = false;
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    pendingComboIndex = 0;
    comboIndex = 0;
    hasPendingHorizontalVelocity = false;
    hasPendingFacingDirection = false;
    pendingHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
    stateMachine.ChangeState(PLAYER_ACTION::HIT);
}

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
    if (actionRuntime.bufferedAction == PLAYER_ACTION::NONE)
    {
        return false;
    }

    const PLAYER_ACTION nextAction_ = actionRuntime.bufferedAction;
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
    const PLAYER_ACTION previousAction_ = previousAction;
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
    actionRuntime.canDodgeCancel = !IsPlayerActionTimelineDriven(action_);
    actionRuntime.canActionBranch = !IsPlayerActionTimelineDriven(action_);
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    timelineActionSafetyTimer = 0.0f;

    SendActionToBlackboard(action_);
}

int PlayerControllerComponent::GetRequestedComboIndex(PLAYER_ACTION nextAction_) const
{
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
    case PLAYER_ACTION::COUNTER_ATTACK:
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
    return airborneActionContextActive && !IsGrounded();
}

bool PlayerControllerComponent::IsDodgeAction(PLAYER_ACTION action_) const
{
    return action_ == PLAYER_ACTION::DODGE ||
        action_ == PLAYER_ACTION::AIR_DODGE;
}

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
    auto& keyConfig_ = KeyConfig::Instance();

    if (keyConfig_.ActionPressedIn(PlayerInputAction::LOCK_ON))
    {
        ToggleLockOn();
    }

    if (keyConfig_.ActionPressedIn(PlayerInputAction::DEBUG_GUARD_HIT) &&
        GetCurrentAction() == PLAYER_ACTION::GUARD)
    {
        NotifyGuardedAttack();
        return;
    }

    const bool isAirborne_ = IsAirborneForAction();
    if (TryDodgeCancelInput()) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::DODGE) &&
        RequestAction(isAirborne_ ? PLAYER_ACTION::AIR_DODGE : PLAYER_ACTION::DODGE)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::JUMP) && RequestAction(PLAYER_ACTION::JUMP)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_A) && RequestAction(PLAYER_ACTION::SKILL_A)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_B) && RequestAction(PLAYER_ACTION::SKILL_B)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::WEAK_ATTACK) &&
        RequestAction(isAirborne_ ? PLAYER_ACTION::AIR_WEAK_ATTACK : PLAYER_ACTION::WEAK_ATTACK)) return;
    if (UpdateStrongAttackInput()) return;

    if (keyConfig_.ActionPressed(PlayerInputAction::GUARD))
    {
        if (RequestAction(PLAYER_ACTION::GUARD)) return;
    }
    else if (GetCurrentAction() == PLAYER_ACTION::GUARD)
    {
        RequestAction(PLAYER_ACTION::IDLE);
        return;
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
    auto owner_ = GetGameObject();
    if (!owner_) return;

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
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_DODGE, group_ == PLAYER_ACTION_GROUP::DODGE);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_JUMP, group_ == PLAYER_ACTION_GROUP::JUMP);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_ACTION, isAirAction_);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_ATTACK, isAirAttack_);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_WEAK_ATTACK, action_ == PLAYER_ACTION::AIR_WEAK_ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_STRONG_ATTACK, action_ == PLAYER_ACTION::AIR_STRONG_ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_STRONG_CHARGE_ATTACK, action_ == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_AIR_DODGE, action_ == PLAYER_ACTION::AIR_DODGE);
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_SKILL,
            attackType_ == PLAYER_ATTACK_TYPE::SKILL_A || attackType_ == PLAYER_ATTACK_TYPE::SKILL_B);
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

    // デバッグ切り替えなので、入力バッファやコンボ待ちを全部リセット
    actionRuntime.canDodgeCancel = true;
    actionRuntime.canActionBranch = true;
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;

    pendingComboIndex = 0;
    comboIndex = 0;

    // ChangeState 内で現在ステートの Exit() が呼ばれる
    stateMachine.ChangeState(action_);
    return true;
}

void PlayerControllerComponent::SendMoveInputToBlackboard(const DirectX::XMFLOAT3& input_)
{
    if (!blackboard) return;

    blackboard->SetVector2(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::MOVE_INPUT, { input_.x, input_.z });
    blackboard->SetVector2(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::MOVE_INPUT, { input_.x, input_.z });
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
    ImGui::Text("State: %s", stateMachine.GetCurrentStateName());
    ImGui::Text("Current Action: %s", GetPlayerActionName(GetCurrentAction()));

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
    ImGui::DragFloat("Timeline Action Fallback", &timelineActionFallbackDuration, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Ground Dodge Cooldown", &groundDodgeCooldownDuration, 0.01f, 0.0f, 5.0f, "%.2f");
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

    ImGui::SeparatorText("Attack Position Assist");
    ImGui::SliderFloat("Position Strength", &attackAssistSettings.positionStrength, 0.0f, 1.0f, "%.2f");
    ImGui::DragFloat("Position Speed", &attackAssistSettings.positionSpeed, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::DragFloat("Max Position Correction", &attackAssistSettings.maxPositionCorrection, 0.01f, 0.0f, 5.0f, "%.2f");
    ImGui::DragFloat("Target Stop Distance", &attackAssistSettings.targetStopDistance, 0.01f, 0.0f, 10.0f, "%.2f");

    ImGui::TextDisabled("Priority: Camera Lock-On > Camera Center > Nearest Enemy");
}

//blackboardに値やカテゴリーを最初に登録する機構
void PlayerControllerComponent::InitializeBlackboardValues()
{
    if (!blackboard)
    {
        auto owner_ = GetGameObject();
        blackboard = owner_ ? owner_->GetOrAddComponent<BlackboardComponent>() : nullptr;
    }

    if (!blackboard) return;

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
        blackboard->SetVector2(category_, PlayerBlackboardKey::DODGE_INPUT, { 0.0f, 0.0f });
        blackboard->SetVector2(category_, PlayerBlackboardKey::HIT_DIRECTION, { 0.0f, 0.0f });
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
    j_["MoveSpeed"] = moveSpeed;
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
    j_["TimelineActionFallbackDuration"] = timelineActionFallbackDuration;
    j_["GroundDodgeCooldownDuration"] = groundDodgeCooldownDuration;
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
    moveSpeed = (std::max)(j_.value("MoveSpeed", moveSpeed), 0.0f);
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
    timelineActionFallbackDuration = (std::max)(
        j_.value("TimelineActionFallbackDuration", timelineActionFallbackDuration),
        0.0f);
    groundDodgeCooldownDuration = (std::max)(
        j_.value("GroundDodgeCooldownDuration", groundDodgeCooldownDuration),
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
        for (const auto& transitionJson_ : transitionsJson_)
        {
            const int from_ = transitionJson_.value("From", static_cast<int>(PLAYER_ACTION::NONE));
            const int to_ = transitionJson_.value("To", static_cast<int>(PLAYER_ACTION::NONE));
            if (from_ < 0 || from_ >= static_cast<int>(PLAYER_ACTION::NONE) ||
                to_ < 0 || to_ >= static_cast<int>(PLAYER_ACTION::NONE))
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
