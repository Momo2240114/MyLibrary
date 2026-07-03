#pragma once
#include "PlayerState.h"

class PlayerIdleState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return "Idle"; }
    void Enter() override;
    void Execute(float elapsedTime_) override;
};
