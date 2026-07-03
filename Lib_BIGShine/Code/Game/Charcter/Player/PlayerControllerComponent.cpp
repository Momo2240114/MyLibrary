#include "PlayerControllerComponent.h"

#include <cmath>

#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Physics/CharacterCollider.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "PlayerActionState.h"
#include "PlayerAttackState.h"
#include "PlayerDodgeState.h"
#include "PlayerHitState.h"
#include "PlayerIdleState.h"
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

}

void PlayerControllerComponent::Start()
{
    auto owner_ = GetGameObject();
    if (!owner_) return;

    EnsurePlayerInputBindings();

    owner_->SetLayer(GameObject::LayerType::Player);
    collider = owner_->GetOrAddComponent<CharacterCollider>();
    movement = owner_->GetOrAddComponent<CharacterMovementComponent>();
    hurtBox = owner_->GetOrAddComponent<HurtBox>();
    if (collider)
    {
        collider->mass = (std::max)(collider->mass, 1.0f);
        collider->RecreatePhysXActor();
    }
    blackboard = owner_->GetOrAddComponent<BlackboardComponent>();
    RegisterPlayerAnimationActions();

    stateMachine.Initialize(*this);
    stateMachine.AddState<PlayerIdleState>(PLAYER_ACTION::IDLE);
    stateMachine.AddState<PlayerMoveState>(PLAYER_ACTION::MOVE);
    stateMachine.AddState<PlayerDodgeState>(PLAYER_ACTION::DODGE);
    stateMachine.AddState<PlayerHitState>(PLAYER_ACTION::HIT);
    stateMachine.AddState<PlayerAttackState>(
        PLAYER_ACTION::WEAK_ATTACK,
        PLAYER_ACTION::WEAK_ATTACK);
    stateMachine.AddState<PlayerAttackState>(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::STRONG_ATTACK);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::GUARD, PLAYER_ACTION::GUARD);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::JUST_GUARD);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::SKILL_A, PLAYER_ACTION::SKILL_A);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::SKILL_B, PLAYER_ACTION::SKILL_B);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::COUNTER_ATTACK, PLAYER_ACTION::COUNTER_ATTACK);

    stateMachine.ChangeState(PLAYER_ACTION::IDLE);
}

void PlayerControllerComponent::Update(float dt_)
{
    stateMachine.Update(dt_);
    ApplyPendingMovement();
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

    const PLAYER_ACTION currentAction_ = GetCurrentAction();
    if (currentAction_ == nextAction_)
    {
        const bool canQueueCombo_ =
            currentAction_ == PLAYER_ACTION::WEAK_ATTACK &&
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
    const bool canUseCancelTransition_ =
        actionRuntime.canCancel &&
        HasCancelTransition(currentAction_, nextAction_);
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
    return std::any_of(
        cancelTransitions.begin(),
        cancelTransitions.end(),
        [from_, to_](const PlayerCancelTransition& transition_)
        {
            return transition_.from == from_ && transition_.to == to_;
        });
}

bool PlayerControllerComponent::AddCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    if (from_ == PLAYER_ACTION::NONE || to_ == PLAYER_ACTION::NONE ||
        from_ == to_ || HasCancelTransition(from_, to_))
    {
        return false;
    }

    cancelTransitions.push_back({ from_, to_ });
    return true;
}

bool PlayerControllerComponent::RemoveCancelTransition(
    PLAYER_ACTION from_,
    PLAYER_ACTION to_)
{
    const auto removeBegin_ = std::remove_if(
        cancelTransitions.begin(),
        cancelTransitions.end(),
        [from_, to_](const PlayerCancelTransition& transition_)
        {
            return transition_.from == from_ && transition_.to == to_;
        });
    if (removeBegin_ == cancelTransitions.end())
    {
        return false;
    }

    cancelTransitions.erase(removeBegin_, cancelTransitions.end());
    return true;
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
    actionRuntime.canCancel = enabled_;
    if (blackboard)
    {
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
    }

    if (enabled_)
    {
        if (actionRuntime.bufferedAction != PLAYER_ACTION::NONE &&
            actionRuntime.bufferedAction != GetCurrentAction() &&
            IsPlayerActionTimelineDriven(GetCurrentAction()) &&
            !actionRuntime.actionEnded)
        {
            NotifyActionEnd();
            return;
        }

        ConsumeBufferedAction();
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

void PlayerControllerComponent::NotifyComboTransition()
{
    SetCanBufferInput(false);
    SetCanCombo(false);
    if (GetCurrentAction() != PLAYER_ACTION::WEAK_ATTACK ||
        actionRuntime.bufferedAction != PLAYER_ACTION::WEAK_ATTACK)
    {
        return;
    }

    pendingComboIndex = actionRuntime.bufferedComboIndex;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    stateMachine.ChangeState(PLAYER_ACTION::WEAK_ATTACK);
}

void PlayerControllerComponent::NotifyActionEnd()
{
    if (!IsPlayerActionTimelineDriven(GetCurrentAction()))
    {
        return;
    }

    actionRuntime.actionEnded = true;
    actionRuntime.canCancel = true;
    if (blackboard)
    {
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
    }

    if (GetCurrentAction() == PLAYER_ACTION::WEAK_ATTACK &&
        actionRuntime.bufferedAction == PLAYER_ACTION::WEAK_ATTACK)
    {
        actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
        actionRuntime.bufferedComboIndex = 0;
    }

    if (ConsumeBufferedAction())
    {
        return;
    }

    const DirectX::XMFLOAT3 input_ = GetMoveInput();
    const PLAYER_ACTION locomotionAction_ =
        (input_.x != 0.0f || input_.z != 0.0f)
        ? PLAYER_ACTION::MOVE
        : PLAYER_ACTION::IDLE;
    stateMachine.ChangeState(locomotionAction_);
}

void PlayerControllerComponent::HandleHitReaction()
{
    actionRuntime.canCancel = false;
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;
    pendingComboIndex = 0;
    comboIndex = 0;
    stateMachine.ChangeState(PLAYER_ACTION::HIT);
}

bool PlayerControllerComponent::ConsumeBufferedAction()
{
    if (actionRuntime.bufferedAction == PLAYER_ACTION::NONE)
    {
        return false;
    }

    const PLAYER_ACTION nextAction_ = actionRuntime.bufferedAction;
    pendingComboIndex = actionRuntime.bufferedComboIndex;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;

    stateMachine.ChangeState(nextAction_);

    return true;
}

void PlayerControllerComponent::EnterAction(PLAYER_ACTION action_)
{
    comboIndex = GetPlayerAttackType(action_) == PLAYER_ATTACK_TYPE::WEAK ? pendingComboIndex : 0;
    pendingComboIndex = 0;
    actionRuntime.canCancel = !IsPlayerActionTimelineDriven(action_);
    actionRuntime.canBufferInput = false;
    actionRuntime.canCombo = false;
    actionRuntime.actionEnded = false;
    actionRuntime.bufferedAction = PLAYER_ACTION::NONE;
    actionRuntime.bufferedComboIndex = 0;

    SendActionToBlackboard(action_);
}

int PlayerControllerComponent::GetRequestedComboIndex(PLAYER_ACTION nextAction_) const
{
    if (nextAction_ != PLAYER_ACTION::WEAK_ATTACK)
    {
        return 0;
    }

    if (GetCurrentAction() == PLAYER_ACTION::WEAK_ATTACK &&
        (actionRuntime.canBufferInput || actionRuntime.canCombo))
    {
        return (comboIndex + 1) % WEAK_ATTACK_COMBO_COUNT;
    }

    return 0;
}

void PlayerControllerComponent::ProcessActionInput()
{
    auto& keyConfig_ = KeyConfig::Instance();

    if (keyConfig_.ActionPressedIn(PlayerInputAction::DEBUG_GUARD_HIT) &&
        GetCurrentAction() == PLAYER_ACTION::GUARD)
    {
        NotifyGuardedAttack();
        return;
    }

    if (keyConfig_.ActionPressedIn(PlayerInputAction::DODGE) && RequestAction(PLAYER_ACTION::DODGE)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_A) && RequestAction(PLAYER_ACTION::SKILL_A)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_B) && RequestAction(PLAYER_ACTION::SKILL_B)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::WEAK_ATTACK) && RequestAction(PLAYER_ACTION::WEAK_ATTACK)) return;
    if (keyConfig_.ActionPressedIn(PlayerInputAction::STRONG_ATTACK) && RequestAction(PLAYER_ACTION::STRONG_ATTACK)) return;

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

    auto& registry_ = owner_->GetLocalRegistry();
    registry_.RegisterAction("Player", "OnHit", [this]()
    {
        HandleHitReaction();
    });
    registry_.RegisterAction("Player", "CanCancelOn", [this]()
        {
            SetCanCancel(true);
        });
    registry_.RegisterAction("Player", "CanCancelOff", [this]()
        {
            SetCanCancel(false);
        });
    registry_.RegisterAction("Player", "InputBufferOn", [this]()
        {
            SetCanBufferInput(true);
        });
    registry_.RegisterAction("Player", "InputBufferOff", [this]()
        {
            SetCanBufferInput(false);
        });
    registry_.RegisterAction("Player", "CanComboOn", [this]()
        {
            SetCanCombo(true);
        });
    registry_.RegisterAction("Player", "CanComboOff", [this]()
        {
            SetCanCombo(false);
        });
    registry_.RegisterAction("Player", "ComboTransition", [this]()
        {
            NotifyComboTransition();
        });
    registry_.RegisterAction("Player", "ActionEnd", [this]()
        {
            NotifyActionEnd();
        });
    registry_.RegisterAction("Player", "NotifyActionEnd", [this]()
        {
            NotifyActionEnd();
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
        blackboard->SetBool(category_, PlayerBlackboardKey::IS_SKILL,
            attackType_ == PLAYER_ATTACK_TYPE::SKILL_A || attackType_ == PLAYER_ATTACK_TYPE::SKILL_B);
        blackboard->SetBool(category_, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
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

bool PlayerControllerComponent::ForceChangeAction(PLAYER_ACTION action_)
{
    if (action_ == PLAYER_ACTION::NONE)
    {
        return false;
    }

    // デバッグ切り替えなので、入力バッファやコンボ待ちを全部リセット
    actionRuntime.canCancel = true;
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


void PlayerControllerComponent::Gui()
{
    ImGui::Text("State: %s", stateMachine.GetCurrentStateName());
    ImGui::Text("Current Action: %s", GetPlayerActionName(GetCurrentAction()));

    ImGui::SeparatorText("Runtime");

    ImGui::Text("Can Cancel: %s", actionRuntime.canCancel ? "true" : "false");
    ImGui::Text("Can Buffer Input: %s", actionRuntime.canBufferInput ? "true" : "false");
    ImGui::Text("Can Combo: %s", actionRuntime.canCombo ? "true" : "false");
    ImGui::Text("Action Ended: %s", actionRuntime.actionEnded ? "true" : "false");
    ImGui::Text("Combo Index: %d", comboIndex);
    ImGui::Text("Buffered Action: %s", GetPlayerActionName(actionRuntime.bufferedAction));

    ImGui::SeparatorText("Cancel Transitions");
    ImGui::Text("Routes: %zu", cancelTransitions.size());
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
            PLAYER_ACTION::GUARD,
            PLAYER_ACTION::JUST_GUARD,
            PLAYER_ACTION::DODGE,
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
}

void PlayerControllerComponent::Serialize(nlohmann::json& j_) const
{
    j_["MoveSpeed"] = moveSpeed;
    j_["CancelTransitions"] = nlohmann::json::array();
    for (const PlayerCancelTransition& transition_ : cancelTransitions)
    {
        j_["CancelTransitions"].push_back(
            {
                { "From", static_cast<int>(transition_.from) },
                { "To", static_cast<int>(transition_.to) }
            });
    }
}

void PlayerControllerComponent::Deserialize(const nlohmann::json& j_)
{
    moveSpeed = j_.value("MoveSpeed", moveSpeed);
    if (j_.contains("CancelTransitions") && j_["CancelTransitions"].is_array())
    {
        cancelTransitions.clear();
        for (const auto& transitionJson_ : j_["CancelTransitions"])
        {
            const int from_ = transitionJson_.value("From", static_cast<int>(PLAYER_ACTION::NONE));
            const int to_ = transitionJson_.value("To", static_cast<int>(PLAYER_ACTION::NONE));
            if (from_ < 0 || from_ >= static_cast<int>(PLAYER_ACTION::NONE) ||
                to_ < 0 || to_ >= static_cast<int>(PLAYER_ACTION::NONE))
            {
                continue;
            }
            cancelTransitions.push_back(
                {
                    static_cast<PLAYER_ACTION>(from_),
                    static_cast<PLAYER_ACTION>(to_)
                });
        }
    }
}

REGISTER_COMPONENT(PlayerControllerComponent, "PlayerController")
