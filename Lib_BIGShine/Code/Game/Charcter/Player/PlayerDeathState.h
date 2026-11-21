#pragma once
#include "PlayerState.h"

// 死亡グラフへ遷移し、倒れた姿勢のままコンテニュー要求を待つPlayerステート。
class PlayerDeathState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override
    {
        return GetPlayerActionName(PLAYER_ACTION::DEAD);
    }

    // 死亡開始フラグとTriggerだけをグラフへ渡す。
    void Enter() override;
    // グラフが公開する実際の再生位置から死亡動作の完了を確認する。
    void Execute(float deltaTime_) override;
    // 消費されていない開始Triggerを次回の死亡へ残さない。
    void Exit() override;

    bool IsAnimationFinished() const
    {
        return animationFinished;
    }

private:
    bool animationFinished = false;
};

// 起き上がりグラフのAnimationEnd遷移を待ち、完了するまで操作を受け付けない。
class PlayerReviveState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override
    {
        return GetPlayerActionName(PLAYER_ACTION::REVIVING);
    }

    // HPを全回復し、死亡グラフへコンテニューTriggerを渡す。
    void Enter() override;
    // 起き上がりへの進入と、グラフによる待機状態への遷移を確認する。
    void Execute(float deltaTime_) override;
    // コンテニューのTriggerを解除する。
    void Exit() override;

    bool IsAnimationFinished() const
    {
        return animationFinished;
    }

private:
    bool animationEntered = false;
    bool animationFinished = false;
};
