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

// 入力方向へ移動し、入力がなくなったらIdleへ戻る。
void PlayerMoveState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    player_->SetHorizontalVelocity(input_, player_->moveSpeed);
    if (input_.x == 0.0f && input_.z == 0.0f)
    {
        // 移動入力が完全に消えたフレームで待機へ戻す。
        player_->RequestAction(PLAYER_ACTION::IDLE);
        return;
    }

    // 移動中でも攻撃、回避、ジャンプなどの入力分岐を受け付ける。
    player_->ProcessActionInput();
}
