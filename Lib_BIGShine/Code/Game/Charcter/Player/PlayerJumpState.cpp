#include "PlayerJumpState.h"

#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Physics/ColliderComponent.h"
#include "PlayerControllerComponent.h"

void PlayerJumpState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    timer = 0.0f;
    player_->EnterAction(PLAYER_ACTION::JUMP);

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (input_.x != 0.0f || input_.z != 0.0f)
    {
        player_->SetFacingDirection(input_);
        player_->SetHorizontalVelocity(input_, player_->moveSpeed * airControlRate);
    }

    if (CharacterMovementComponent* movement_ = player_->GetMovement())
    {
        movement_->AddImpulse({ 0.0f, player_->jumpImpulse, 0.0f });
    }
}

void PlayerJumpState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    timer += elapsedTime_;

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (input_.x != 0.0f || input_.z != 0.0f)
    {
        player_->SetHorizontalVelocity(input_, player_->moveSpeed * airControlRate);
    }

    player_->ProcessActionInput();

    ColliderComponent* collider_ = player_->GetCollider();
    if (timer >= minAirTime && collider_ && collider_->isGrounded)
    {
        const PLAYER_ACTION nextAction_ =
            (input_.x != 0.0f || input_.z != 0.0f)
            ? PLAYER_ACTION::MOVE
            : PLAYER_ACTION::IDLE;
        player_->RequestAction(nextAction_);
    }
}

void PlayerJumpState::Exit()
{
    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
    }
}
