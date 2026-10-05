#include "PrecompiledHeader.h"
#include "PlayerSkillState.h"
#include "PlayerControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

PlayerSkillState::PlayerSkillState(const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_, PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_), action(action_)
{
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
        // 未定義の既存単独Skill入力では従来のグラフ駆動を維持する。
        SyncBlackboard();
        return;
    }
    active = true;
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    execution.Start(GetOwner(), *definition_, [this, player_](const SkillPhase& phase_)
    {
        // Phaseデータは共通とし、Playerの受付窓だけを既存APIへ反映する。
        player_->SetCanBufferInput(phase_.canBufferInput);
        player_->SetCanCombo(false);
        player_->SetCanDodgeCancel(phase_.canDodgeCancel);
        if (!active) return;
        player_->SetCanActionBranch(phase_.canActionBranch);
    });
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
        SyncBlackboard();
    }
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

void PlayerSkillState::Exit()
{
    BS_PROFILE_SCOPE("PlayerSkillState::Exit");
    execution.Stop();
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
