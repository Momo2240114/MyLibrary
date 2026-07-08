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
    float timer = 0.0f;
    float minAirTime = 0.12f;
    float airControlRate = 0.72f;
};
