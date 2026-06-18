#include "PlayerDodgeState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

void PlayerDodgeState::Enter()
{
    auto owner_ = GetOwner();
    playerComponent = owner_ ? owner_->GetComponent<PlayerControllerComponent>() : nullptr;

    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::DODGE);
    //player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);

    dodgeDirection = player_->GetMoveInput();
    if (dodgeDirection.x == 0.0f && dodgeDirection.z == 0.0f)
    {
        dodgeDirection = player_->GetCameraForwardDirection();
    }
}

void PlayerDodgeState::Execute(float)
{
    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(dodgeDirection);
    player_->ProcessActionInput();
}

void PlayerDodgeState::Exit()
{
    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}