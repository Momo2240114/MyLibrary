#pragma once

#include <DirectXMath.h>

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerDodgeState : public PlayerState
{
public:
    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::DODGE); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

private:
    std::weak_ptr<PlayerControllerComponent> playerComponent;
    DirectX::XMFLOAT3 dodgeDirection{ 0.0f, 0.0f, 1.0f };
};
