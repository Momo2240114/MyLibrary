#include "PlayerActionState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

PlayerActionState::PlayerActionState(PLAYER_ACTION action_) :
    action(action_)
{
}

void PlayerActionState::Enter()
{
    auto owner_ = GetOwner();
    playerComponent = owner_ ? owner_->GetComponent<PlayerControllerComponent>() : nullptr;

    auto player_ = playerComponent.lock();
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
    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}
