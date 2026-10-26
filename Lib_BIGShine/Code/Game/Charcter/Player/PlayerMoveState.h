#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

// 移動入力がある間、カメラ基準の水平移動をPlayerへ適用するState。
class PlayerMoveState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::MOVE); }
    // 移動Actionの開始をControllerとアニメーションへ通知する。
    void Enter() override;
    // 入力方向へ移動を予約し、入力終了時は待機へ、それ以外は行動入力へ進む。
    void Execute(float elapsedTime_) override;
};
