#include "PlayerAttackState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

PlayerAttackState::PlayerAttackState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

void PlayerAttackState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;
    player_->EnterAction(action);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

void PlayerAttackState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    (void)elapsedTime_;
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

void PlayerAttackState::Exit()
{
    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
    }
}
