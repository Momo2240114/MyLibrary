#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerMoveState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::MOVE); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
};
