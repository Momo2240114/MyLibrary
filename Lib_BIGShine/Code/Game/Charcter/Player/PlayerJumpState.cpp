#include "PlayerJumpState.h"

#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>

void PlayerJumpState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    const bool resumeLoop_ = player_->ConsumeJumpLoopResumeRequest();
    hasLeftGround = resumeLoop_;
    phase = resumeLoop_
        ? PLAYER_JUMP_RUNTIME_PHASE::LOOP
        : PLAYER_JUMP_RUNTIME_PHASE::START;
    player_->BeginAirborneActionContext();
    player_->EnterAction(PLAYER_ACTION::JUMP);
    player_->SendJumpPhaseToBlackboard(
        resumeLoop_
        ? PLAYER_LOCOMOTION_PHASE::JUMP_LOOP
        : PLAYER_LOCOMOTION_PHASE::JUMP_START);

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (!resumeLoop_ && (input_.x != 0.0f || input_.z != 0.0f))
    {
        player_->SetFacingDirection(input_);
        player_->SetHorizontalVelocity(input_, player_->moveSpeed * player_->airControlRate);
    }

    if (!resumeLoop_)
    {
        if (CharacterMovementComponent* movement_ = player_->GetMovement())
        {
            movement_->AddImpulse({ 0.0f, player_->jumpImpulse, 0.0f });
        }
    }
}

void PlayerJumpState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (CharacterMovementComponent* movement_ = player_->GetMovement())
    {
        const DirectX::XMFLOAT3 currentVelocity_ = movement_->GetDesiredHorizontalVelocity();
        DirectX::XMFLOAT3 targetVelocity_{ 0.0f, 0.0f, 0.0f };
        const bool hasMoveInput_ = input_.x != 0.0f || input_.z != 0.0f;
        if (hasMoveInput_)
        {
            targetVelocity_ =
            {
                input_.x * player_->moveSpeed * player_->airControlRate,
                0.0f,
                input_.z * player_->moveSpeed * player_->airControlRate
            };
            movement_->SetFacingDirection(input_, false);
        }

        const float blendRate_ = hasMoveInput_
            ? player_->airControlAcceleration
            : player_->airDeceleration;
        const float blend_ = std::clamp(blendRate_ * elapsedTime_, 0.0f, 1.0f);
        DirectX::XMFLOAT3 nextVelocity_
        {
            currentVelocity_.x + (targetVelocity_.x - currentVelocity_.x) * blend_,
            0.0f,
            currentVelocity_.z + (targetVelocity_.z - currentVelocity_.z) * blend_
        };

        const float speedSquared_ =
            nextVelocity_.x * nextVelocity_.x +
            nextVelocity_.z * nextVelocity_.z;
        if (speedSquared_ < 0.0001f)
        {
            nextVelocity_ = { 0.0f, 0.0f, 0.0f };
        }

        movement_->SetHorizontalVelocity(nextVelocity_);
    }

    player_->ProcessActionInput();

    const bool isGrounded_ = player_->IsGrounded();
    if (!isGrounded_)
    {
        hasLeftGround = true;
        if (phase == PLAYER_JUMP_RUNTIME_PHASE::START)
        {
            ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::LOOP);
        }
        return;
    }

    if (hasLeftGround)
    {
        ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::END);
        if (input_.x != 0.0f || input_.z != 0.0f)
        {
            player_->RequestAction(PLAYER_ACTION::MOVE);
        }
    }
}

void PlayerJumpState::Exit()
{
    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        if (!hasLeftGround && player_->IsGrounded())
        {
            player_->EndAirborneActionContext();
        }
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
    }
}

void PlayerJumpState::ChangePhase(PLAYER_JUMP_RUNTIME_PHASE phase_)
{
    if (phase == phase_) return;

    phase = phase_;

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
