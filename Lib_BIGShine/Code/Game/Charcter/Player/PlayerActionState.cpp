#include "PrecompiledHeader.h"

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

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (action == PLAYER_ACTION::GUARD)
    {
        const DirectX::XMFLOAT3 guardInput_ =
            player_->GetGuardMoveInput(input_);
        player_->SendGuardMoveInputToBlackboard(guardInput_);
        player_->SetHorizontalVelocity(input_, player_->guardMoveSpeed, false);
        player_->ApplyGuardFacingToTarget();
    }
    player_->ProcessActionInput();
}
