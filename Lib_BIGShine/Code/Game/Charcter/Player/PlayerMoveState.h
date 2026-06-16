#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerMoveState : public PlayerState
{
public:
    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::MOVE); }
    void Enter() override;
    void Execute(float elapsedTime_) override;

private:
    std::weak_ptr<PlayerControllerComponent> playerComponent;
};
