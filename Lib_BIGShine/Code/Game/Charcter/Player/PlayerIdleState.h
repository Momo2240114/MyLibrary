#pragma once
#include "PlayerState.h"

// 入力がない通常待機状態。移動入力や攻撃入力を待ち受けて次Actionへ渡す。
class PlayerIdleState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return "Idle"; }
    void Enter() override;
    void Execute(float elapsedTime_) override;
};
