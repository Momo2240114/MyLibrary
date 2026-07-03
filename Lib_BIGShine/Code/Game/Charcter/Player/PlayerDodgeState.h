#pragma once

#include <DirectXMath.h>

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerDodgeState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::DODGE); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

private:
    void SendDodgeInputToBlackboard(const DirectX::XMFLOAT3& input_);

    DirectX::XMFLOAT3 dodgeDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 dodgeBlendInput{ 0.0f, 0.0f, 1.0f };
};
