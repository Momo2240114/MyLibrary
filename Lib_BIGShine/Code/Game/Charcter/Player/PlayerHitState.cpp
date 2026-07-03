#include "PlayerHitState.h"

#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>

void PlayerHitState::Enter()
{
    elapsedTime = 0.0f;

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::HIT);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);

    const std::shared_ptr<AnimationControllerComponent> animation_ =
        GetComponent<AnimationControllerComponent>();
    if (animation_)
    {
        animation_->SetUseTransitionGraph(false);
        animation_->ChangeState("SS_Hit_R", 0.05f);
    }
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
    const std::shared_ptr<AnimationControllerComponent> animation_ =
        GetComponent<AnimationControllerComponent>();
    if (animation_)
    {
        animation_->SetUseTransitionGraph(true);
    }
    elapsedTime = 0.0f;
}
