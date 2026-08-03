#include "PrecompiledHeader.h"

#include "PlayerActionState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "PlayerControllerComponent.h"

// 扱うAction種別を受け取り、共通ActionStateとして生成する。
PlayerActionState::PlayerActionState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

// Action開始をControllerへ通知し、ガード系やカウンターでは水平移動を止める。
void PlayerActionState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(action);
    if (action == PLAYER_ACTION::GUARD ||
        action == PLAYER_ACTION::JUST_GUARD ||
        action == PLAYER_ACTION::COUNTER_ATTACK)
    {
        player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    }
}

// ガード中は入力をガード基準へ変換し、それ以外は共通のAction入力処理を進める。
void PlayerActionState::Execute(float)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (action == PLAYER_ACTION::GUARD)
    {
        // ガード中はTargetを基準にした前後左右入力へ変換して、防御移動を安定させる。
        const DirectX::XMFLOAT3 guardInput_ =
            player_->GetGuardMoveInput(input_);
        player_->SendGuardMoveInputToBlackboard(guardInput_);
        player_->SetHorizontalVelocity(input_, player_->guardMoveSpeed, false);
        player_->ApplyGuardFacingToTarget();
    }
    player_->ProcessActionInput();
}
