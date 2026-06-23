#include "PlayerDodgeState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"
#include "Engine/Physics/ColliderComponent.h"

void PlayerDodgeState::Enter()
{
    auto owner_ = GetOwner();
    playerComponent = owner_ ? owner_->GetComponent<PlayerControllerComponent>() : nullptr;

    auto player_ = playerComponent.lock();
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
    player_->SendDodgeInputToBlackboard(dodgeBlendInput);

    // Dodge開始時に縦速度を殺す
    auto collider_ = owner_ ? owner_->GetComponent<ColliderComponent>() : nullptr;
    if (collider_)
    {
        collider_->velocity.y = 0.0f;
    }
}

void PlayerDodgeState::Execute(float)
{
    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(dodgeDirection);
    player_->SendDodgeInputToBlackboard(dodgeBlendInput);
    player_->ProcessActionInput();
}

void PlayerDodgeState::Exit()
{
    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}
