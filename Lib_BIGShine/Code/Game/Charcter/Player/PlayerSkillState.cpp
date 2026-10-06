#include "PrecompiledHeader.h"
#include "PlayerSkillState.h"
#include "PlayerControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/GamePlay/Combat/HitReactionComponent.h"
#include <functional>
#include <limits>

PlayerSkillState::PlayerSkillState(const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_, PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_), action(action_)
{
}

PlayerSkillState::~PlayerSkillState()
{
    // Controllerの破棄中は参照を戻さず、残っているComponentの一時設定だけを解除する。
    for (const auto& snapshot_ : guardSnapshots)
        if (const auto guard_ = snapshot_.collider.lock())
        {
            if (snapshot_.enabled) guard_->Activate(); else guard_->Deactivate();
            guard_->SetJustGuardEnabled(snapshot_.justGuard);
        }
    if (armorApplied)
        if (const auto reaction_ = protectedHitReaction.lock()) reaction_->SetArmor(previousReactionArmor);
}

void PlayerSkillState::Enter()
{
    BS_PROFILE_SCOPE("PlayerSkillState::Enter");
    auto* player_ = GetPlayerController();
    if (!player_) return;
    finishing = false;
    active = false;
    player_->EnterAction(action);
    const auto* definition_ = player_->GetSkillDefinition(action);
    if (!definition_)
    {
        // 空スロットや参照先不足では旧固定Skillへ流さず、通常終了経路へ戻す。
        active = true;        finishing = true;
        SyncBlackboard();

        return;
    }
    active = true;
    // DebugやBufferによる直接State遷移でも、使用不可のSkillを実行しない。
    if (!player_->CanExecuteSkill(action))
    {
        finishing = true;
        SyncBlackboard();
        return;
    }
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    const bool started_ = execution.Start(GetOwner(), *definition_, [this, player_](const SkillPhase& phase_)
    {
        ApplyPhaseDefence(phase_);
        // Phaseデータは共通とし、Playerの受付窓だけを既存APIへ反映する。
        player_->SetCanBufferInput(phase_.canBufferInput);
        player_->SetCanCombo(false);
        player_->SetCanDodgeCancel(phase_.canDodgeCancel);
        if (!active) return;
        player_->SetCanActionBranch(phase_.canActionBranch);
    });
    if (started_ && execution.GetResult() != SkillExecutionResult::PHASE_START_FAILED)
        player_->NotifySkillStarted(action);
    SyncBlackboard();
}

void PlayerSkillState::Execute(float elapsedTime_)
{
    BS_PROFILE_SCOPE("PlayerSkillState::Execute");
    auto* player_ = GetPlayerController();
    if (!player_) return;
    if (active)
    {
        execution.Update(elapsedTime_);
        if (!active) return;
        if (!execution.IsActive())
        {
            finishing = true;
            player_->NotifyActionEnd();
            return;
        }
        // 突進中の向きだけは既存LockOn補正を再利用し、入力で通常Moveへは移さない。
        const auto* phase_ = execution.GetCurrentPhase();
        if (phase_ && phase_->forwardSpeed != 0.0f) player_->ApplyGuardFacingToTarget();
        SyncBlackboard();
    }
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

void PlayerSkillState::Exit()
{
    BS_PROFILE_SCOPE("PlayerSkillState::Exit");
    execution.Stop();
    RestoreDefence();
    active = false;
    finishing = false;
    SyncBlackboard();
}

void PlayerSkillState::NotifyPhaseEnd()
{
    execution.NotifyPhaseEnd();
}

void PlayerSkillState::SyncBlackboard()
{
    const auto blackboard_ = GetComponent<BlackboardComponent>();
    if (!blackboard_) return;
    blackboard_->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::IS_SKILL_ACTIVE, active);
    blackboard_->SetInt(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::SKILL_PHASE, active ? execution.GetPhaseIndex() : -1);
    blackboard_->SetString(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CURRENT_SKILL,
        active ? execution.GetSkillName() : "");
}

void PlayerSkillState::StopSkill()
{
    BS_PROFILE_SCOPE("PlayerSkillState::StopSkill");
    if (!active || finishing) return;
    execution.Stop();
    if (!active) return;
    finishing = true;
    if (auto* player_ = GetPlayerController()) player_->NotifyActionEnd();
}

// Player固有の防御をデータで切り替え、スキル名による分岐は作らない。
void PlayerSkillState::ApplyPhaseDefence(const SkillPhase& phase_)
{
    BS_PROFILE_SCOPE("PlayerSkillState::ApplyPhaseDefence");
    auto* player_ = GetPlayerController();
    if (!player_) return;
    if (!phase_.enableGuard && guardApplied)
    {
        for (const auto& snapshot_ : guardSnapshots)
            if (const auto guard_ = snapshot_.collider.lock())
            {
                if (snapshot_.enabled) guard_->Activate(); else guard_->Deactivate();
                guard_->SetJustGuardEnabled(snapshot_.justGuard);
            }
        guardSnapshots.clear();
        guardApplied = false;
    }
    if (phase_.enableGuard && !guardApplied)
    {
        std::function<void(const std::shared_ptr<GameObject>&)> collect_;
        collect_ = [&](const std::shared_ptr<GameObject>& object_)
        {
            if (!object_) return;
            for (const auto& guard_ : object_->GetComponents<GuardCollider>())
            {
                guardSnapshots.push_back({ guard_, guard_->IsEnabled(), guard_->IsJustGuardEnabled() });
                guard_->SetJustGuardEnabled(false);
                guard_->Activate();
            }
            if (object_->transform)
                for (const auto& child_ : object_->transform->children)
                    if (child_) collect_(child_->gameObject.lock());
        };
        collect_(GetOwner());
        guardApplied = true;
    }
    if (phase_.preventFlinch && !armorApplied)
    {
        const auto owner_ = GetOwner();
        const auto reaction_ = owner_ ? owner_->GetComponent<HitReactionComponent>() : nullptr;
        protectedHitReaction = reaction_;
        previousReactionArmor = reaction_ ? reaction_->GetArmor() : 0.0f;
        previousArmor = player_->GetCurrentArmor();
        previousArmorOverride = player_->HasRuntimeArmorOverride();
        player_->SetRuntimeArmor((std::numeric_limits<float>::max)());
        armorApplied = true;
    }
    else if (!phase_.preventFlinch && armorApplied)
    {
        if (previousArmorOverride) player_->SetRuntimeArmor(previousArmor); else player_->ResetRuntimeArmor();
        armorApplied = false;
    }
}

void PlayerSkillState::RestoreDefence()
{
    SkillPhase phase_;
    ApplyPhaseDefence(phase_);
}

bool PlayerSkillState::PreventsHitReaction() const
{
    const auto* phase_ = execution.GetCurrentPhase();
    return active && phase_ && phase_->preventFlinch;
}
