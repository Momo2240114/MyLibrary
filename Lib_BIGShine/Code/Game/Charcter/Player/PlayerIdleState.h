#pragma once
#include "PlayerState.h"

class PlayerIdleState : public PlayerState
{
public:
    const char* GetName() const override { return "Idle"; }
    void Enter(PlayerControllerComponent& player_) override;
    void Update(PlayerControllerComponent& player_, float dt_) override;
};