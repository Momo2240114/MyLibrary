#pragma once
#include "PlayerState.h"

class PlayerIdleState : public PlayerState
{
public:
    const char* GetName() const override { return "Idle"; }
    void Enter() override;
    void Execute(float elapsedTime_) override;

private:
    std::weak_ptr<PlayerControllerComponent> playerComponent;
};
