#include "PrecompiledHeader.h"

#include "AIDirectorComponent.h"

#include "Game/GameMode/GameModeManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <imgui.h>

void AIDirectorComponent::Start()
{
    UpdateRates();
    UpdatePolicy();

    if (applyToGameMode)
    {
        ApplyPolicyToGameMode();
    }
}

void AIDirectorComponent::Update(float deltaTime_)
{
    (void)deltaTime_;

    if (!isRunning) return;

    UpdateRates();
    UpdatePolicy();

    if (applyToGameMode)
    {
        ApplyPolicyToGameMode();
    }
}

void AIDirectorComponent::Gui()
{
    ImGui::Checkbox(IM_U8("実行##AIDirectorComponent_IsRunning"), &isRunning);
    ImGui::SameLine();
    ImGui::Checkbox(IM_U8("GameModeへ反映##AIDirectorComponent_ApplyToGameMode"), &applyToGameMode);

    if (ImGui::Button(IM_U8("統計リセット##AIDirectorComponent_ResetStats")))
    {
        ResetStats();
    }

    ImGui::SeparatorText(IM_U8("統計##AIDirectorComponent_Stats"));

    ImGui::Text("Attack Used      : %d", stats.totalEnemyAttackUsed);
    ImGui::Text("Attack Hit       : %d", stats.totalEnemyAttackHit);
    ImGui::Text("Dodged           : %d", stats.totalEnemyAttackDodged);
    ImGui::Text("Just Dodged      : %d", stats.totalEnemyAttackJustDodged);
    ImGui::Text("Guarded          : %d", stats.totalEnemyAttackGuarded);
    ImGui::Text("Just Guarded     : %d", stats.totalEnemyAttackJustGuarded);
    ImGui::Text("Player Damaged   : %d", stats.playerDamagedCount);
    ImGui::Text("Total Damage     : %.2f", stats.totalPlayerDamage);

    ImGui::SeparatorText(IM_U8("Rate##AIDirectorComponent_Rates"));

    ImGui::Text("Hit Rate         : %.2f", rates.hitRate);
    ImGui::Text("Dodge Rate       : %.2f", rates.dodgeRate);
    ImGui::Text("Just Dodge Rate  : %.2f", rates.justDodgeRate);
    ImGui::Text("Guard Rate       : %.2f", rates.guardRate);
    ImGui::Text("Just Guard Rate  : %.2f", rates.justGuardRate);

    ImGui::SeparatorText(IM_U8("方針##AIDirectorComponent_Policy"));

    ImGui::Text("Prefer Delayed Attack : %s", policy.preferDelayedAttack ? "true" : "false");
    ImGui::Text("Prefer Feint Attack   : %s", policy.preferFeintAttack ? "true" : "false");
    ImGui::Text("Prefer Guard Break    : %s", policy.preferGuardBreak ? "true" : "false");
    ImGui::Text("Reduce Aggression     : %s", policy.reduceAggression ? "true" : "false");

    ImGui::Text("Attack Delay Rate : %.2f", policy.attackDelayRate);
    ImGui::Text("Feint Rate        : %.2f", policy.feintRate);
    ImGui::Text("Guard Break Rate  : %.2f", policy.guardBreakRate);
    ImGui::Text("Aggression Rate   : %.2f", policy.aggressionRate);

    ImGui::SeparatorText(IM_U8("しきい値##AIDirectorComponent_Thresholds"));

    ImGui::DragFloat(
        IM_U8("高JustDodge率##AIDirectorComponent_HighJustDodgeRateThreshold"),
        &highJustDodgeRateThreshold,
        0.01f,
        0.0f,
        1.0f);

    ImGui::DragFloat(
        IM_U8("高Guard率##AIDirectorComponent_HighGuardRateThreshold"),
        &highGuardRateThreshold,
        0.01f,
        0.0f,
        1.0f);

    ImGui::DragFloat(
        IM_U8("低Hit率##AIDirectorComponent_LowHitRateThreshold"),
        &lowHitRateThreshold,
        0.01f,
        0.0f,
        1.0f);

    ImGui::InputInt(
        IM_U8("被弾回数しきい値##AIDirectorComponent_PlayerDamagedCountThreshold"),
        &playerDamagedCountThreshold);

    ImGui::DragFloat(
        IM_U8("最大AttackDelayRate##AIDirectorComponent_MaxAttackDelayRate"),
        &maxAttackDelayRate,
        0.01f,
        1.0f,
        3.0f);

    ImGui::DragFloat(
        IM_U8("最大FeintRate##AIDirectorComponent_MaxFeintRate"),
        &maxFeintRate,
        0.01f,
        0.0f,
        1.0f);

    ImGui::DragFloat(
        IM_U8("最大GuardBreakRate##AIDirectorComponent_MaxGuardBreakRate"),
        &maxGuardBreakRate,
        0.01f,
        0.0f,
        1.0f);

    ImGui::DragFloat(
        IM_U8("最小Aggression##AIDirectorComponent_AggressionMin"),
        &aggressionMin,
        0.01f,
        0.0f,
        1.0f);
}

void AIDirectorComponent::Serialize(nlohmann::json& json_) const
{
    json_["IsRunning"] = isRunning;
    json_["ApplyToGameMode"] = applyToGameMode;

    json_["HighJustDodgeRateThreshold"] = highJustDodgeRateThreshold;
    json_["HighGuardRateThreshold"] = highGuardRateThreshold;
    json_["LowHitRateThreshold"] = lowHitRateThreshold;
    json_["PlayerDamagedCountThreshold"] = playerDamagedCountThreshold;

    json_["MaxAttackDelayRate"] = maxAttackDelayRate;
    json_["MaxFeintRate"] = maxFeintRate;
    json_["MaxGuardBreakRate"] = maxGuardBreakRate;
    json_["AggressionMin"] = aggressionMin;
    json_["AggressionDefault"] = aggressionDefault;
}

void AIDirectorComponent::Deserialize(const nlohmann::json& json_)
{
    isRunning = json_.value("IsRunning", true);
    applyToGameMode = json_.value("ApplyToGameMode", true);

    highJustDodgeRateThreshold = json_.value("HighJustDodgeRateThreshold", 0.5f);
    highGuardRateThreshold = json_.value("HighGuardRateThreshold", 0.5f);
    lowHitRateThreshold = json_.value("LowHitRateThreshold", 0.25f);
    playerDamagedCountThreshold = json_.value("PlayerDamagedCountThreshold", 5);

    maxAttackDelayRate = json_.value("MaxAttackDelayRate", 1.35f);
    maxFeintRate = json_.value("MaxFeintRate", 0.35f);
    maxGuardBreakRate = json_.value("MaxGuardBreakRate", 0.4f);
    aggressionMin = json_.value("AggressionMin", 0.65f);
    aggressionDefault = json_.value("AggressionDefault", 1.0f);

    UpdateRates();
    UpdatePolicy();

    if (applyToGameMode)
    {
        ApplyPolicyToGameMode();
    }
}

void AIDirectorComponent::ResetStats()
{
    stats = {};
    rates = {};
    policy = {};
    policy.attackDelayRate = 1.0f;
    policy.aggressionRate = aggressionDefault;

    if (applyToGameMode)
    {
        ApplyPolicyToGameMode();
    }
}

void AIDirectorComponent::NotifyEnemyAttackUsed(const std::string& attackName_)
{
    (void)attackName_;
    ++stats.totalEnemyAttackUsed;
}

void AIDirectorComponent::NotifyEnemyAttackHit(const std::string& attackName_)
{
    (void)attackName_;
    ++stats.totalEnemyAttackHit;
}

void AIDirectorComponent::NotifyEnemyAttackDodged(const std::string& attackName_)
{
    (void)attackName_;
    ++stats.totalEnemyAttackDodged;
}

void AIDirectorComponent::NotifyEnemyAttackJustDodged(const std::string& attackName_)
{
    (void)attackName_;
    ++stats.totalEnemyAttackJustDodged;
}

void AIDirectorComponent::NotifyEnemyAttackGuarded(const std::string& attackName_)
{
    (void)attackName_;
    ++stats.totalEnemyAttackGuarded;
}

void AIDirectorComponent::NotifyEnemyAttackJustGuarded(const std::string& attackName_)
{
    (void)attackName_;
    ++stats.totalEnemyAttackJustGuarded;
}

void AIDirectorComponent::NotifyPlayerDamaged(float damage_)
{
    ++stats.playerDamagedCount;
    stats.totalPlayerDamage += damage_;
}

GameModeManager* AIDirectorComponent::ResolveGameModeManager()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    return owner_->GetManager().GetGameModeManager();
}

void AIDirectorComponent::UpdateRates()
{
    const int totalUsed_ = stats.totalEnemyAttackUsed;

    rates.hitRate = SafeRate(stats.totalEnemyAttackHit, totalUsed_);
    rates.dodgeRate = SafeRate(stats.totalEnemyAttackDodged, totalUsed_);
    rates.justDodgeRate = SafeRate(stats.totalEnemyAttackJustDodged, totalUsed_);
    rates.guardRate = SafeRate(stats.totalEnemyAttackGuarded, totalUsed_);
    rates.justGuardRate = SafeRate(stats.totalEnemyAttackJustGuarded, totalUsed_);
}

void AIDirectorComponent::UpdatePolicy()
{
    policy.preferDelayedAttack = false;
    policy.preferFeintAttack = false;
    policy.preferGuardBreak = false;
    policy.reduceAggression = false;

    policy.attackDelayRate = 1.0f;
    policy.feintRate = 0.0f;
    policy.guardBreakRate = 0.0f;
    policy.aggressionRate = aggressionDefault;

    if (stats.totalEnemyAttackUsed <= 0)
    {
        return;
    }

    const bool playerReadsAttack_ =
        rates.justDodgeRate >= highJustDodgeRateThreshold ||
        rates.hitRate <= lowHitRateThreshold;

    if (playerReadsAttack_)
    {
        policy.preferDelayedAttack = true;
        policy.preferFeintAttack = true;
        policy.attackDelayRate = maxAttackDelayRate;
        policy.feintRate = maxFeintRate;
    }

    if (rates.guardRate >= highGuardRateThreshold ||
        rates.justGuardRate >= highGuardRateThreshold)
    {
        policy.preferGuardBreak = true;
        policy.guardBreakRate = maxGuardBreakRate;
    }

    if (stats.playerDamagedCount >= playerDamagedCountThreshold)
    {
        policy.reduceAggression = true;
        policy.aggressionRate = aggressionMin;

        if (policy.preferFeintAttack)
        {
            policy.feintRate *= 0.5f;
        }

        if (policy.preferDelayedAttack)
        {
            policy.attackDelayRate = 1.0f + (policy.attackDelayRate - 1.0f) * 0.5f;
        }
    }
}

void AIDirectorComponent::ApplyPolicyToGameMode()
{
    GameModeManager* gameModeManager_ = ResolveGameModeManager();
    if (!gameModeManager_) return;

    gameModeManager_->SetAIDirectorPreferDelayedAttack(policy.preferDelayedAttack);
    gameModeManager_->SetAIDirectorPreferFeintAttack(policy.preferFeintAttack);
    gameModeManager_->SetAIDirectorPreferGuardBreak(policy.preferGuardBreak);
    gameModeManager_->SetAIDirectorReduceAggression(policy.reduceAggression);

    gameModeManager_->SetAIDirectorAttackDelayRate(policy.attackDelayRate);
    gameModeManager_->SetAIDirectorFeintRate(policy.feintRate);
    gameModeManager_->SetAIDirectorGuardBreakRate(policy.guardBreakRate);
    gameModeManager_->SetAIDirectorAggressionRate(policy.aggressionRate);
}

float AIDirectorComponent::SafeRate(int value_, int total_) const
{
    if (total_ <= 0) return 0.0f;
    return static_cast<float>(value_) / static_cast<float>(total_);
}

REGISTER_COMPONENT(AIDirectorComponent, "AIDirectorComponent")