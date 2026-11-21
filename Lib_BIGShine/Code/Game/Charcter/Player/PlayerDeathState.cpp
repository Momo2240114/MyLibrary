#include "PrecompiledHeader.h"
#include "PlayerDeathState.h"
#include "PlayerControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"

void PlayerDeathState::Enter()
{
    BS_PROFILE_SCOPE("PlayerDeathState::Enter");
    animationFinished = false;
    if (const auto player_ = GetPlayerController())
    {
        player_->EnterAction(PLAYER_ACTION::DEAD);
    }
    if (const auto board_ = GetComponent<BlackboardComponent>())
    {
        board_->SetBool("Anim", "DeathSequenceActive", true);
    }
    if (const auto movement_ = GetComponent<CharacterMovementComponent>())
    {
        movement_->ResetMovementRequests();
    }
    if (const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetOwner()))
    {
        graph_->ClearTrigger("PlayerContinue");
        graph_->RequestTrigger("PlayerDied");
        graph_->RequestEvaluation();
    }
}

void PlayerDeathState::Execute(float deltaTime_)
{
    BS_PROFILE_SCOPE("PlayerDeathState::Execute");
    (void)deltaTime_;
    const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetOwner());
    const auto state_ = graph_ ? graph_->GetActiveState() : nullptr;
    if (!state_ || state_->name != "PlayerDeath")
    {
        return;
    }
    const float duration_ = graph_->GetPlaybackDuration();
    if (duration_ > 0.0f && graph_->GetPlaybackTime() >= duration_)
    {
        animationFinished = true;
    }
}

void PlayerDeathState::Exit()
{
    if (const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetOwner()))
    {
        graph_->ClearTrigger("PlayerDied");
    }
}

void PlayerReviveState::Enter()
{
    BS_PROFILE_SCOPE("PlayerReviveState::Enter");
    animationEntered = false;
    animationFinished = false;
    if (const auto player_ = GetPlayerController())
    {
        player_->EnterAction(PLAYER_ACTION::REVIVING);
    }
    if (const auto health_ = GetComponent<HealthComponent>())
    {
        health_->ResetHealth();
    }
    if (const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetOwner()))
    {
        graph_->RequestTrigger("PlayerContinue");
        graph_->RequestEvaluation();
    }
}

void PlayerReviveState::Execute(float deltaTime_)
{
    BS_PROFILE_SCOPE("PlayerReviveState::Execute");
    (void)deltaTime_;
    const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetOwner());
    const auto state_ = graph_ ? graph_->GetActiveState() : nullptr;
    if (!state_)
    {
        return;
    }
    if (state_->name == "PlayerRevive")
    {
        animationEntered = true;
    }
    // 再生秒数をコードへ埋め込まず、グラフのAnimationEndによる離脱を完了通知として扱う。
    if (animationEntered && state_->name == "SS_Idle")
    {
        animationFinished = true;
    }
}

void PlayerReviveState::Exit()
{
    if (const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetOwner()))
    {
        graph_->ClearTrigger("PlayerContinue");
    }
}
