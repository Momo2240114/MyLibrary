#include "PlayerJumpState.h"

#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Physics/ColliderComponent.h"
#include "PlayerControllerComponent.h"

void PlayerJumpState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    timer = 0.0f;
    phaseTimer = 0.0f;
    hasLeftGround = false;
    phase = PLAYER_JUMP_RUNTIME_PHASE::START;
    player_->EnterAction(PLAYER_ACTION::JUMP);
    player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_START);

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
    phaseTimer += elapsedTime_;

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (input_.x != 0.0f || input_.z != 0.0f)
    {
        player_->SetHorizontalVelocity(input_, player_->moveSpeed * airControlRate);
    }

    player_->ProcessActionInput();

    ColliderComponent* collider_ = player_->GetCollider();
    const bool isGrounded_ = player_->IsGrounded();
    if (!isGrounded_)
    {
        hasLeftGround = true;
    }

    if (phase == PLAYER_JUMP_RUNTIME_PHASE::END)
    {
        if (phaseTimer >= player_->jumpEndDuration)
        {
            const PLAYER_ACTION nextAction_ =
                (input_.x != 0.0f || input_.z != 0.0f)
                ? PLAYER_ACTION::MOVE
                : PLAYER_ACTION::IDLE;
            player_->RequestAction(nextAction_);
        }
        return;
    }

    const float verticalVelocity_ = collider_ ? collider_->velocity.y : 0.0f;
    if (phase == PLAYER_JUMP_RUNTIME_PHASE::START &&
        phaseTimer >= player_->jumpStartMinDuration &&
        verticalVelocity_ <= 0.0f)
    {
        ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::LOOP);
    }

    if (hasLeftGround && isGrounded_)
    {
        ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::END);
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

void PlayerJumpState::ChangePhase(PLAYER_JUMP_RUNTIME_PHASE phase_)
{
    if (phase == phase_) return;

    phase = phase_;
    phaseTimer = 0.0f;

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    switch (phase)
    {
    case PLAYER_JUMP_RUNTIME_PHASE::START:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_START);
        break;
    case PLAYER_JUMP_RUNTIME_PHASE::LOOP:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_LOOP);
        break;
    case PLAYER_JUMP_RUNTIME_PHASE::END:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_END);
        break;
    default:
        break;
    }
}
