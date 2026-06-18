#include "PlayerAttackState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

PlayerAttackState::PlayerAttackState(PLAYER_ACTION action_) :
    action(action_)
{
}

void PlayerAttackState::Enter()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return;

    const std::shared_ptr<PlayerControllerComponent> player_ =
        owner_->GetComponent<PlayerControllerComponent>();
    playerComponent = player_;

    if (!player_) return;
    player_->EnterAction(action);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

void PlayerAttackState::Execute(float elapsedTime_)
{
    const std::shared_ptr<PlayerControllerComponent> player_ = playerComponent.lock();
    if (!player_) return;

    (void)elapsedTime_;
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

void PlayerAttackState::Exit()
{
    if (const std::shared_ptr<PlayerControllerComponent> player_ = playerComponent.lock())
    {
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
    }
}
