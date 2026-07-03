#include "PlayerIdleState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

void PlayerIdleState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::IDLE);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

void PlayerIdleState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}
