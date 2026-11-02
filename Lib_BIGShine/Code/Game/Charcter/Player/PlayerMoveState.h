#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

// カメラ基準の地上移動を、開始・走行・停止まで同じAction内で管理するState。
class PlayerMoveState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::MOVE); }
    // 移動Actionの開始をControllerとアニメーションへ通知する。
    void Enter() override;
    // 戦闘入力を先に受け付け、速度更新はControllerで一度だけ行う。
    void Execute(float elapsedTime_) override;
};
