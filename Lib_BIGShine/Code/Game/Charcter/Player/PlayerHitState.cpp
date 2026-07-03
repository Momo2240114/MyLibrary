#include "PlayerHitState.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>
#include <cmath>

void PlayerHitState::Enter()
{
    elapsedTime = 0.0f;

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::HIT);

    const std::shared_ptr<CharacterMovementComponent> movement_ =
        GetComponent<CharacterMovementComponent>();
    if (movement_)
    {
        movement_->StopHorizontalMovementImmediately();
    }

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    DirectX::SimpleMath::Vector2 hitDirection_{ 0.0f, 0.0f };
    blackboard_->GetVector2("Hit", "Direction", hitDirection_);

    blackboard_->SetBool(
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardKey::HIT_TRIGGER,
        true);

    blackboard_->SetBool(
        PlayerBlackboardCategory::ANIM,
        PlayerBlackboardKey::HIT_TRIGGER,
        true);

    blackboard_->SetVector2(
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardKey::HIT_DIRECTION,
        hitDirection_);

    blackboard_->SetVector2(
        PlayerBlackboardCategory::ANIM,
        PlayerBlackboardKey::HIT_DIRECTION,
        hitDirection_);
}

void PlayerHitState::Execute(float elapsedTime_)
{
    elapsedTime += (std::max)(elapsedTime_, 0.0f);
    if (elapsedTime < fallbackDuration) return;

    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        player_->NotifyActionEnd();
    }
}

void PlayerHitState::Exit()
{
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (blackboard_)
    {
        blackboard_->SetBool(
            PlayerBlackboardCategory::PLAYER,
            PlayerBlackboardKey::HIT_TRIGGER,
            false);

        blackboard_->SetBool(
            PlayerBlackboardCategory::ANIM,
            PlayerBlackboardKey::HIT_TRIGGER,
            false);
    }

    elapsedTime = 0.0f;
}
