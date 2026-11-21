#include "PrecompiledHeader.h"

#include "PlayerHitState.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>

// Hitカテゴリから被弾情報を読み取り、Player/Animカテゴリへ反応用の値を書き込む。
void PlayerHitState::Enter()
{
    elapsedTime = 0.0f;
    airHitReaction = false;
    airHitRecoverable = false;
    airHitKnockdown = false;

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_)
    {
        return;
    }

    player_->EnterAction(PLAYER_ACTION::HIT);

    const std::shared_ptr<CharacterMovementComponent> movement_ =
        GetComponent<CharacterMovementComponent>();
    if (movement_)
    {
        movement_->StopHorizontalMovementImmediately();
    }

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        return;
    }

    DirectX::SimpleMath::Vector2 hitDirection_{ 0.0f, 0.0f };
    float impactPower_ = 0.0f;
    float armor_ = player_->GetCurrentArmor();
    blackboard_->GetVector2("Hit", "Direction", hitDirection_);
    blackboard_->GetFloat("Hit", "ImpactPower", impactPower_);
    blackboard_->GetFloat("Hit", "Armor", armor_);

    impactPower_ = (std::max)(impactPower_, 0.0f);
    armor_ = (std::max)(armor_, 0.0f);

    // 接地状態と衝撃値から、軽/中/重/空中ノックダウンを判定する。
    airHitReaction = !player_->IsGrounded();

    int hitReactionLevel_ = 0;
    if (impactPower_ >= player_->hitHeavyImpactThreshold)
    {
        hitReactionLevel_ = 2;
    }
    else if (impactPower_ >= player_->hitMediumImpactThreshold)
    {
        hitReactionLevel_ = 1;
    }
    else if (airHitReaction)
    {
        hitReactionLevel_ = 1;
    }

    const bool isHitLight_ = hitReactionLevel_ == 0;
    const bool isHitMedium_ = hitReactionLevel_ == 1;
    const bool isHitHeavy_ = hitReactionLevel_ >= 2;

    airHitKnockdown =
        airHitReaction &&
        impactPower_ >= player_->airHitKnockdownImpactThreshold;
    airHitRecoverable = airHitReaction && !airHitKnockdown;

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::HIT_TRIGGER,
            true);

        blackboard_->SetVector2(
            category_,
            PlayerBlackboardKey::HIT_DIRECTION,
            hitDirection_);

        blackboard_->SetFloat(
            category_,
            PlayerBlackboardKey::HIT_IMPACT_POWER,
            impactPower_);

        blackboard_->SetFloat(
            category_,
            PlayerBlackboardKey::HIT_ARMOR,
            armor_);

        blackboard_->SetInt(
            category_,
            PlayerBlackboardKey::HIT_REACTION_LEVEL,
            hitReactionLevel_);

        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::IS_HIT_LIGHT,
            isHitLight_);

        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::IS_HIT_MEDIUM,
            isHitMedium_);

        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::IS_HIT_HEAVY,
            isHitHeavy_);

        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::IS_AIR_HIT_REACTION,
            airHitReaction);

        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::IS_AIR_HIT_RECOVERABLE,
            airHitRecoverable);

        blackboard_->SetBool(
            category_,
            PlayerBlackboardKey::IS_AIR_HIT_KNOCKDOWN,
            airHitKnockdown);
    }
}

// HitReaction中は基本的にTimeline終了を待ち、保険時間を超えたらAction終了にする。
void PlayerHitState::Execute(float elapsedTime_)
{
    elapsedTime += (std::max)(elapsedTime_, 0.0f);

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_)
    {
        return;
    }

    if (airHitKnockdown && !player_->IsGrounded())
    {
        // 空中ノックダウンは接地するまで終了させない。
        return;
    }

    if (airHitKnockdown)
    {
        // 接地後もAnimation側の終了通知を優先するため、自動終了しない。
        return;
    }

    if (elapsedTime < fallbackDuration)
    {
        return;
    }

    player_->NotifyActionEnd();
}

// Hit用Triggerと反応フラグを消し、次の被弾へ状態が残らないようにする。
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

        constexpr const char* categories_[] =
        {
            PlayerBlackboardCategory::PLAYER,
            PlayerBlackboardCategory::ANIM
        };

        for (const char* category_ : categories_)
        {
            blackboard_->SetFloat(category_, PlayerBlackboardKey::HIT_IMPACT_POWER, 0.0f);
            blackboard_->SetInt(category_, PlayerBlackboardKey::HIT_REACTION_LEVEL, 0);
            blackboard_->SetBool(category_, PlayerBlackboardKey::IS_HIT_LIGHT, false);
            blackboard_->SetBool(category_, PlayerBlackboardKey::IS_HIT_MEDIUM, false);
            blackboard_->SetBool(category_, PlayerBlackboardKey::IS_HIT_HEAVY, false);
            blackboard_->SetBool(category_, PlayerBlackboardKey::IS_AIR_HIT_REACTION, false);
            blackboard_->SetBool(category_, PlayerBlackboardKey::IS_AIR_HIT_RECOVERABLE, false);
            blackboard_->SetBool(category_, PlayerBlackboardKey::IS_AIR_HIT_KNOCKDOWN, false);
        }
    }

    elapsedTime = 0.0f;
    airHitReaction = false;
    airHitRecoverable = false;
    airHitKnockdown = false;
}
