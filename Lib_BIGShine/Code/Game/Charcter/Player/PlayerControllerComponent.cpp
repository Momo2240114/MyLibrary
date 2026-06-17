#include "PlayerControllerComponent.h"

#include <cmath>

#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "PlayerActionState.h"
#include "PlayerDodgeState.h"
#include "PlayerIdleState.h"
#include "PlayerMoveState.h"

namespace
{
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
    auto& keyConfig_ = KeyConfig::Instance();
    if (keyConfig_.GetConfigFile().empty())
    {
        keyConfig_.SetConfigFile("Data/Input/KeyConfig.json");
        keyConfig_.Load();
    }

    const auto hasAction_ = [&keyConfig_](const char* actionName_)
        {
            return keyConfig_.GetActionMap().find(actionName_) != keyConfig_.GetActionMap().end();
        };
    const auto hasAxis_ = [&keyConfig_](const char* axisName_)
        {
            return keyConfig_.GetAxisMap().find(axisName_) != keyConfig_.GetAxisMap().end();
        };

    if (!hasAxis_(PlayerInputAction::MOVE_X))
    {
        keyConfig_.SetAxisBinds(PlayerInputAction::MOVE_X,
            {
                AxisUnit{ AxisSourceType::Button, DeviceType::KBM, KeyInput::Key::K_A, -1.0f },
                AxisUnit{ AxisSourceType::Button, DeviceType::KBM, KeyInput::Key::K_D, 1.0f },
                AxisUnit{ AxisSourceType::GamepadStick, DeviceType::Gamepad, static_cast<int>(AxisCode::LeftStickX), 1.0f, 0.15f }
            });
    }

    if (!hasAxis_(PlayerInputAction::MOVE_Z))
    {
        keyConfig_.SetAxisBinds(PlayerInputAction::MOVE_Z,
            {
                AxisUnit{ AxisSourceType::Button, DeviceType::KBM, KeyInput::Key::K_S, -1.0f },
                AxisUnit{ AxisSourceType::Button, DeviceType::KBM, KeyInput::Key::K_W, 1.0f },
                AxisUnit{ AxisSourceType::GamepadStick, DeviceType::Gamepad, static_cast<int>(AxisCode::LeftStickY), 1.0f, 0.15f }
            });
    }

    if (!hasAction_(PlayerInputAction::WEAK_ATTACK))
    {
        keyConfig_.SetBinds(PlayerInputAction::WEAK_ATTACK, { InputCombo{ InputUnit{ DeviceType::KBM, VK_LBUTTON } } });
    }
    if (!hasAction_(PlayerInputAction::STRONG_ATTACK))
    {
        keyConfig_.SetBinds(PlayerInputAction::STRONG_ATTACK, { InputCombo{ InputUnit{ DeviceType::KBM, VK_RBUTTON } } });
    }
    if (!hasAction_(PlayerInputAction::GUARD))
    {
        keyConfig_.SetBinds(PlayerInputAction::GUARD, { InputCombo{ InputUnit{ DeviceType::KBM, KeyInput::Key::K_Q } } });
    }
    if (!hasAction_(PlayerInputAction::DODGE))
    {
        keyConfig_.SetBinds(PlayerInputAction::DODGE, { InputCombo{ InputUnit{ DeviceType::KBM, KeyInput::Key::K_Shift } } });
    }
    if (!hasAction_(PlayerInputAction::SKILL_A))
    {
        keyConfig_.SetBinds(PlayerInputAction::SKILL_A, { InputCombo{ InputUnit{ DeviceType::KBM, KeyInput::Key::K_E } } });
    }
    if (!hasAction_(PlayerInputAction::SKILL_B))
    {
        keyConfig_.SetBinds(PlayerInputAction::SKILL_B, { InputCombo{ InputUnit{ DeviceType::KBM, KeyInput::Key::K_R } } });
    }
    if (!hasAction_(PlayerInputAction::DEBUG_GUARD_HIT))
    {
        keyConfig_.SetBinds(PlayerInputAction::DEBUG_GUARD_HIT, { InputCombo{ InputUnit{ DeviceType::KBM, KeyInput::Key::K_F } } });
    }
}

void PlayerControllerComponent::Start()
{
    auto owner_ = GetGameObject();
    if (!owner_) return;

    EnsurePlayerInputBindings();

    owner_->SetLayer(GameObject::LayerType::Player);
    collider = owner_->GetComponent<ColliderComponent>();
    if (collider)
    {
        collider->collisionRole = ColliderComponent::CollisionRole::CHARACTER;
        collider->usePhysicsResponse = true;
        collider->physicsControlsTransform = true;
        collider->isTrigger = false;
        collider->mass = (std::max)(collider->mass, 1.0f);
        collider->RecreatePhysXActor();
    }
    blackboard = owner_->GetOrAddComponent<BlackboardComponent>();
    RegisterPlayerAnimationActions();

    stateMachine.AddState<PlayerIdleState>(PLAYER_ACTION::IDLE);
    stateMachine.AddState<PlayerMoveState>(PLAYER_ACTION::MOVE);
    stateMachine.AddState<PlayerDodgeState>(PLAYER_ACTION::DODGE);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::WEAK_ATTACK);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::STRONG_ATTACK, PLAYER_ACTION::STRONG_ATTACK);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::GUARD, PLAYER_ACTION::GUARD);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::JUST_GUARD, PLAYER_ACTION::JUST_GUARD);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::SKILL_A, PLAYER_ACTION::SKILL_A);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::SKILL_B, PLAYER_ACTION::SKILL_B);
    stateMachine.AddState<PlayerActionState>(PLAYER_ACTION::COUNTER_ATTACK, PLAYER_ACTION::COUNTER_ATTACK);

    stateMachine.ChangeState(*this, PLAYER_ACTION::IDLE);
}

void PlayerControllerComponent::Update(float dt_)
{
    stateMachine.Update(dt_);
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetMoveInput() const
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

    return GetCameraRelativeMoveDirection(gameObject.lock(), input_);
}

DirectX::XMFLOAT3 PlayerControllerComponent::GetCameraForwardDirection() const
{
    return ::GetCameraForwardDirection(gameObject.lock());
}

void PlayerControllerComponent::SetHorizontalVelocity(
    const DirectX::XMFLOAT3& dir_,
    float speed_)
{
    if (!collider) return;

    const float inputLengthSq_ =
        dir_.x * dir_.x +
        dir_.z * dir_.z;

    if (inputLengthSq_ < 0.0001f)
    {
        collider->velocity.x = 0.0f;
        collider->velocity.z = 0.0f;
        return;
    }

    collider->velocity.x = dir_.x * speed_;
    collider->velocity.z = dir_.z * speed_;
}

bool PlayerControllerComponent::RequestAction(PLAYER_ACTION nextAction_)
{
    if (!CanTransitionPlayerAction(GetCurrentAction(), nextAction_))
    {
        return false;
    }

    const int requestedComboIndex_ = GetRequestedComboIndex(nextAction_);
    if (!actionRuntime.canCancel)
    {
        if (actionRuntime.canBufferInput || actionRuntime.canCombo)
        {
            actionRuntime.bufferedAction = nextAction_;
            actionRuntime.bufferedComboIndex = requestedComboIndex_;
        }
        return false;
    }

    pendingComboIndex = requestedComboIndex_;
    stateMachine.ChangeState(*this, nextAction_);
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

void PlayerControllerComponent::NotifyActionEnd()
{
    actionRuntime.actionEnded = true;
    actionRuntime.canCancel = true;
    if (blackboard)
    {
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
        blackboard->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::ACTION_ENDED, actionRuntime.actionEnded);
        blackboard->SetBool(PlayerBlackboardCategory::ANIM, PlayerBlackboardKey::CAN_CANCEL, actionRuntime.canCancel);
    }

    if (ConsumeBufferedAction())
    {
        return;
    }

    const DirectX::XMFLOAT3 input_ = GetMoveInput();
    RequestAction((input_.x != 0.0f || input_.z != 0.0f) ? PLAYER_ACTION::MOVE : PLAYER_ACTION::IDLE);
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

    if (!CanTransitionPlayerAction(GetCurrentAction(), nextAction_))
    {
        return false;
    }

    stateMachine.ChangeState(*this, nextAction_);
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

    if (GetCurrentAction() == PLAYER_ACTION::WEAK_ATTACK && actionRuntime.canCombo)
    {
        return comboIndex + 1;
    }

    return 0;
}

void PlayerControllerComponent::ProcessActionInput()
{
    auto& keyConfig_ = KeyConfig::Instance();

    //if (keyConfig_.ActionPressedIn(PlayerInputAction::DEBUG_GUARD_HIT) &&
    //    GetCurrentAction() == PLAYER_ACTION::GUARD)
    //{
    //    NotifyGuardedAttack();
    //    return;
    //}

    //if (keyConfig_.ActionPressedIn(PlayerInputAction::DODGE) && RequestAction(PLAYER_ACTION::DODGE)) return;
    //if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_A) && RequestAction(PLAYER_ACTION::SKILL_A)) return;
    //if (keyConfig_.ActionPressedIn(PlayerInputAction::SKILL_B) && RequestAction(PLAYER_ACTION::SKILL_B)) return;
    //if (keyConfig_.ActionPressedIn(PlayerInputAction::WEAK_ATTACK) && RequestAction(PLAYER_ACTION::WEAK_ATTACK)) return;
    //if (keyConfig_.ActionPressedIn(PlayerInputAction::STRONG_ATTACK) && RequestAction(PLAYER_ACTION::STRONG_ATTACK)) return;

    //if (keyConfig_.ActionPressed(PlayerInputAction::GUARD))
    //{
    //    if (RequestAction(PLAYER_ACTION::GUARD)) return;
    //}
    //else if (GetCurrentAction() == PLAYER_ACTION::GUARD)
    //{
    //    RequestAction(PLAYER_ACTION::IDLE);
    //    return;
    //}

    const DirectX::XMFLOAT3 input_ = GetMoveInput();
    if ((input_.x != 0.0f || input_.z != 0.0f) && RequestAction(PLAYER_ACTION::MOVE))
    {
        return;
    }
}

void PlayerControllerComponent::RegisterPlayerAnimationActions()
{
    auto owner_ = GetGameObject();
    if (!owner_) return;

    auto& registry_ = owner_->GetLocalRegistry();
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
    SendMoveInputToBlackboard(GetMoveInput());
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
    ImGui::Text("Can Cancel: %s", actionRuntime.canCancel ? "true" : "false");
    ImGui::Text("Can Buffer Input: %s", actionRuntime.canBufferInput ? "true" : "false");
    ImGui::Text("Can Combo: %s", actionRuntime.canCombo ? "true" : "false");
    ImGui::Text("Combo Index: %d", comboIndex);
    ImGui::Text("Buffered Action: %s", GetPlayerActionName(actionRuntime.bufferedAction));
    ImGui::TextUnformatted("Input: KeyConfig Player.* actions / axes");
    ImGui::DragFloat("Move Speed", &moveSpeed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Dodge Speed", &dodgeSpeed, 0.1f, 0.0f, 100.0f);
}

void PlayerControllerComponent::Serialize(nlohmann::json& j_) const
{
    j_["MoveSpeed"] = moveSpeed;
    j_["DodgeSpeed"] = dodgeSpeed;
}

void PlayerControllerComponent::Deserialize(const nlohmann::json& j_)
{
    moveSpeed = j_.value("MoveSpeed", moveSpeed);
    dodgeSpeed = j_.value("DodgeSpeed", dodgeSpeed);
}

REGISTER_COMPONENT(PlayerControllerComponent, "PlayerController")
