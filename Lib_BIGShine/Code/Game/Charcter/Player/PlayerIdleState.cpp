#include "PrecompiledHeader.h"

#include "PlayerIdleState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

// 待機Actionへ入り、前Stateから残った水平速度を止める。
void PlayerIdleState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::IDLE);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

// 移動入力をBlackboardへ送り、Controller側の入力処理で次Actionを判定する。
void PlayerIdleState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}
