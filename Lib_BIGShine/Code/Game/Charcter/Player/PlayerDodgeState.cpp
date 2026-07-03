#include "PlayerDodgeState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/Physics/ColliderComponent.h"
#include "PlayerControllerComponent.h"

void PlayerDodgeState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::DODGE);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);

    const DirectX::XMFLOAT3 cameraForward_ = player_->GetCameraForwardDirection();
    player_->SetFacingDirection(cameraForward_);

    dodgeDirection = player_->GetMoveInput();
    dodgeBlendInput = player_->GetMoveInputAxis();
    if (dodgeDirection.x == 0.0f && dodgeDirection.z == 0.0f)
    {
        // 入力なしなら後ろ回避
        dodgeDirection =
        {
            -cameraForward_.x,
            0.0f,
            -cameraForward_.z
        };

        // Back Dodgeアニメを選ばせる
        dodgeBlendInput = { 0.0f, 0.0f, -1.0f };
    }
    SendDodgeInputToBlackboard(dodgeBlendInput);

    // Dodge開始時に縦速度を殺す
    if (ColliderComponent* collider_ = player_->GetCollider())
    {
        collider_->velocity.y = 0.0f;
    }
}

void PlayerDodgeState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(dodgeDirection);
    SendDodgeInputToBlackboard(dodgeBlendInput);
    player_->ProcessActionInput();
}

void PlayerDodgeState::Exit()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

void PlayerDodgeState::SendDodgeInputToBlackboard(
    const DirectX::XMFLOAT3& input_)
{
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetVector2(
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardKey::DODGE_INPUT,
        { input_.x, input_.z });
    blackboard_->SetVector2(
        PlayerBlackboardCategory::ANIM,
        PlayerBlackboardKey::DODGE_INPUT,
        { input_.x, input_.z });
}
