#include "PlayerMoveState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"
#include "Engine/Math/Vector.h"

void PlayerMoveState::Enter()
{
    auto owner_ = GetOwner();
    playerComponent = owner_ ? owner_->GetComponent<PlayerControllerComponent>() : nullptr;

    auto player_ = playerComponent.lock();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::MOVE);
}

void PlayerMoveState::Execute(float)
{
    auto player_ = playerComponent.lock();
    if (!player_) return;

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    player_->SetHorizontalVelocity(input_, player_->moveSpeed);
    if (input_.x == 0.0f && input_.z == 0.0f)
    {
        player_->RequestAction(PLAYER_ACTION::IDLE);
        return;
    }

    player_->ProcessActionInput();
}
