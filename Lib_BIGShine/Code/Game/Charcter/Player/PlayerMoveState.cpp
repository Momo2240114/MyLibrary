#include "PrecompiledHeader.h"

#include "PlayerMoveState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"
#include "Engine/Math/Vector.h"

// 移動ActionとしてBlackboardとAnimationへ現在状態を通知する。
void PlayerMoveState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::MOVE);
}

// 開始・走行・停止のすべてで戦闘入力を受け付け、速度更新はControllerへ委ねる。
void PlayerMoveState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->ProcessActionInput();
    // 移動更新はController終端へ集約し、EnterとExecuteによる二重加速を防ぐ。
}
