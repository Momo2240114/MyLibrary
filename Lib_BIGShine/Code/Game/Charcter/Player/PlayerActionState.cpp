#include "PlayerActionState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

PlayerActionState::PlayerActionState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

void PlayerActionState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(action);
    if (action == PLAYER_ACTION::GUARD ||
        action == PLAYER_ACTION::JUST_GUARD ||
        action == PLAYER_ACTION::COUNTER_ATTACK)
    {
        player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    }
}

void PlayerActionState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}
