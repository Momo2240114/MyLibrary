#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerActionState : public PlayerState
{
public:
    explicit PlayerActionState(PLAYER_ACTION action_);

    const char* GetName() const override { return GetPlayerActionName(action); }
    void Enter() override;
    void Execute(float elapsedTime_) override;

private:
    std::weak_ptr<PlayerControllerComponent> playerComponent;
    PLAYER_ACTION action = PLAYER_ACTION::NONE;
};
