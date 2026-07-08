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

private:
    enum class PLAYER_JUMP_RUNTIME_PHASE
    {
        START,
        LOOP,
        END
    };

    void ChangePhase(PLAYER_JUMP_RUNTIME_PHASE phase_);

    PLAYER_JUMP_RUNTIME_PHASE phase = PLAYER_JUMP_RUNTIME_PHASE::START;
    float timer = 0.0f;
    float phaseTimer = 0.0f;
    float airControlRate = 0.72f;
    bool hasLeftGround = false;
};
