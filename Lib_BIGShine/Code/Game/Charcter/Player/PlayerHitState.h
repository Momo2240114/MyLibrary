#pragma once

#include "PlayerState.h"

// 被弾時のHitReactionをBlackboardへ反映し、軽/中/重/空中の反応を選ぶState。
class PlayerHitState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override
    {
        return GetPlayerActionName(PLAYER_ACTION::HIT);
    }

    // 被弾情報と接地状態から、地上・空中の反応フラグを設定する。
    void Enter() override;
    // 通常の被弾は保険時間で終了し、空中ノックダウンは終了通知を待つ。
    void Execute(float elapsedTime_) override;
    // 被弾用のTriggerと反応フラグを両Blackboardカテゴリから解除する。
    void Exit() override;

private:
    // HitStateに入ってからの経過時間。
    float elapsedTime = 0.0f;
    // Timeline終了通知が来ない場合にActionを終える保険時間。
    float fallbackDuration = 0.6f;
    // 被弾時点で空中にいたか。
    bool airHitReaction = false;
    // 空中被弾後に復帰可能な反応か。
    bool airHitRecoverable = false;
    // 空中被弾でノックダウン扱いにするか。
    bool airHitKnockdown = false;
};
