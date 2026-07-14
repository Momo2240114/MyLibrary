#pragma once

#include "PlayerState.h"

class PlayerHitState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::HIT); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

private:
    float elapsedTime = 0.0f;
    float fallbackDuration = 0.6f;
    bool airHitReaction = false;
    bool airHitRecoverable = false;
    bool airHitKnockdown = false;
};
