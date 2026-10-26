#pragma once
#include "PlayerState.h"

// 入力がない通常待機状態。移動入力や攻撃入力を待ち受けて次Actionへ渡す。
class PlayerIdleState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return "Idle"; }
    // 待機Actionを通知し、前の状態から残った水平移動を停止する。
    void Enter() override;
    // 現在の移動入力を公開し、Controllerの入力優先順で次の行動を受け付ける。
    void Execute(float elapsedTime_) override;
};
