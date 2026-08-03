#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

// 移動入力がある間、カメラ基準の水平移動をPlayerへ適用するState。
class PlayerMoveState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::MOVE); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
};
