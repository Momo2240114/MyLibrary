#pragma once

#include "PlayerState.h"

class PlayerJumpState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::JUMP); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;
    void NotifyJumpLoopRequested();

private:
    enum class PLAYER_JUMP_RUNTIME_PHASE
    {
        START,
        DOUBLE_JUMP,
        LOOP,
        END
    };

    void TryStartDoubleJump(
        PlayerControllerComponent& player_,
        const DirectX::XMFLOAT3& input_);
    void ChangePhase(PLAYER_JUMP_RUNTIME_PHASE phase_);
    static const char* GetJumpRuntimePhaseName(PLAYER_JUMP_RUNTIME_PHASE phase_);

    PLAYER_JUMP_RUNTIME_PHASE phase = PLAYER_JUMP_RUNTIME_PHASE::START;
    float phaseTimer = 0.0f;
    int jumpCount = 0;
    bool hasLeftGround = false;
    bool hasEnteredDoubleJumpAnimation = false;
};
