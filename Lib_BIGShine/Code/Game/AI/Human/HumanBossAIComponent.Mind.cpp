#include "PrecompiledHeader.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"
#include "HumanBossAIComponent.h"
#include "HumanBossActionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include <algorithm>
#include <cmath>

namespace
{
    struct MindParameter
    {
        const char* key;
        float HumanBossMindSettings::* value;
        float minimum;
        float maximum;
    };
    constexpr MindParameter PARAMETERS[] =
    {
        { "ReactionDelay", &HumanBossMindSettings::reactionDelay, 0.01f, 1.0f },
        { "DodgeDelayMultiplier", &HumanBossMindSettings::dodgeDelayMultiplier, 0.5f, 3.0f },
        { "GuardDelayMultiplier", &HumanBossMindSettings::guardDelayMultiplier, 0.5f, 3.0f },
        { "JustGuardDelayMultiplier", &HumanBossMindSettings::justGuardDelayMultiplier, 0.5f, 3.0f },
        { "RepeatPredictionDiscount", &HumanBossMindSettings::repeatPredictionDiscount, 0.0f, 0.8f },
        { "ReactionBudgetMax", &HumanBossMindSettings::reactionBudgetMax, 0.0f, 20.0f },
        { "ReactionBudgetRecovery", &HumanBossMindSettings::reactionBudgetRecovery, 0.0f, 10.0f },
        { "DodgeCost", &HumanBossMindSettings::dodgeCost, 0.0f, 10.0f },
        { "GuardCost", &HumanBossMindSettings::guardCost, 0.0f, 10.0f },
        { "JustGuardCost", &HumanBossMindSettings::justGuardCost, 0.0f, 10.0f },
        { "SameReactionPenalty", &HumanBossMindSettings::sameReactionPenalty, 0.01f, 1.0f },
        { "ReadErrorChance", &HumanBossMindSettings::readErrorChance, 0.01f, 1.0f },
        { "DecisionLockTime", &HumanBossMindSettings::decisionLockTime, 0.05f, 3.0f },
        { "GuardReadThreshold", &HumanBossMindSettings::guardReadThreshold, 0.0f, 1.0f },
        { "GuardHoldThreshold", &HumanBossMindSettings::guardHoldThreshold, 0.0f, 5.0f },
        { "DodgeReadThreshold", &HumanBossMindSettings::dodgeReadThreshold, 0.0f, 1.0f },
        { "WeakReadBoost", &HumanBossMindSettings::weakReadBoost, 0.0f, 10.0f },
        { "GuardReadBoost", &HumanBossMindSettings::guardReadBoost, 0.0f, 10.0f },
        { "DodgeReadBoost", &HumanBossMindSettings::dodgeReadBoost, 0.0f, 10.0f },
        { "PunishChance", &HumanBossMindSettings::punishChance, 0.0f, 0.9f },
        { "LightCounterChance", &HumanBossMindSettings::lightCounterChance, 0.0f, 0.9f },
        { "WhiffWindow", &HumanBossMindSettings::whiffWindow, 0.05f, 3.0f },
        { "BaitChance", &HumanBossMindSettings::baitChance, 0.0f, 0.9f },
        { "BaitWait", &HumanBossMindSettings::baitWait, 0.05f, 3.0f },
        { "FeintChance", &HumanBossMindSettings::feintChance, 0.0f, 0.8f },
        { "FeintCooldown", &HumanBossMindSettings::feintCooldown, 1.0f, 60.0f },
        { "FeintRecentPenalty", &HumanBossMindSettings::feintRecentPenalty, 0.01f, 1.0f },
        { "DodgePunishDelay", &HumanBossMindSettings::dodgePunishDelay, 0.0f, 1.5f },
        { "MaximumComboDelay", &HumanBossMindSettings::maximumComboDelay, 0.1f, 3.0f },
        { "HitEscapeRecovery", &HumanBossMindSettings::hitEscapeRecovery, 0.05f, 1.0f },
        { "HitEscapeProtection", &HumanBossMindSettings::hitEscapeProtection, 0.0f, 2.0f },
        { "PostHitRepositionTime", &HumanBossMindSettings::postHitRepositionTime, 0.0f, 3.0f },
        { "AirHeavyThreatHeightTolerance", &HumanBossMindSettings::airHeavyThreatHeightTolerance, 0.5f, 6.0f },
        { "AirHeavyReactionDelayMultiplier", &HumanBossMindSettings::airHeavyReactionDelayMultiplier, 0.1f, 1.0f },
        { "AirHeavyPressureWindow", &HumanBossMindSettings::airHeavyPressureWindow, 0.1f, 5.0f },
        { "AirHeavyMemoryWindow", &HumanBossMindSettings::airHeavyMemoryWindow, 0.5f, 10.0f },
        { "AirLandingPressureDistance", &HumanBossMindSettings::airLandingPressureDistance, 1.0f, 5.0f },
        { "AirHeavyChaseSpeedMultiplier", &HumanBossMindSettings::airHeavyChaseSpeedMultiplier, 1.0f, 2.0f },
        { "AirHeavyRecoveryArmor", &HumanBossMindSettings::airHeavyRecoveryArmor, 0.0f, 100.0f }
    };
}

const char* GetHumanBossIntentName(HumanBossIntent intent_)
{
    constexpr const char* NAMES[] = { "NONE", "OBSERVE", "PRESSURE", "BAIT", "PUNISH", "DEFEND", "REPOSITION", "DISENGAGE", "CHASE", "BREAK_GUARD", "PUNISH_DODGE", "COUNTER_ATTACK" };
    return NAMES[std::clamp(static_cast<int>(intent_), 0, 11)];
}

void HumanBossMindSettings::Gui()
{
    for (const auto& parameter_ : PARAMETERS)
    {
        ImGui::DragFloat(parameter_.key, &(this->*parameter_.value), 0.01f, parameter_.minimum, parameter_.maximum);
        this->*parameter_.value = std::clamp(this->*parameter_.value, parameter_.minimum, parameter_.maximum);
    }
    ImGui::DragFloat3("Phase Delay Multipliers", phaseDelay.data(), 0.05f, 0.1f, 3.0f);
    ImGui::DragFloat3("Phase Observation Multipliers", phaseObserve.data(), 0.05f, 0.1f, 3.0f);
    ImGui::DragFloat3("Phase Aggression Multipliers", phaseAggression.data(), 0.05f, 0.1f, 3.0f);
    for (auto* values_ : { &phaseDelay, &phaseObserve, &phaseAggression }) for (auto& value_ : *values_) value_ = std::clamp(value_, 0.1f, 3.0f);
}

void HumanBossMindSettings::Serialize(nlohmann::json& json_) const
{
    for (const auto& parameter_ : PARAMETERS) json_[parameter_.key] = this->*parameter_.value;
    json_["PhaseDelay"] = phaseDelay;
    json_["PhaseObserve"] = phaseObserve;
    json_["PhaseAggression"] = phaseAggression;
}

void HumanBossMindSettings::Deserialize(const nlohmann::json& json_)
{
    for (const auto& parameter_ : PARAMETERS) this->*parameter_.value = std::clamp(json_.value(parameter_.key, this->*parameter_.value), parameter_.minimum, parameter_.maximum);
    phaseDelay = json_.value("PhaseDelay", phaseDelay);
    phaseObserve = json_.value("PhaseObserve", phaseObserve);
    phaseAggression = json_.value("PhaseAggression", phaseAggression);
    for (auto* values_ : { &phaseDelay, &phaseObserve, &phaseAggression }) for (auto& value_ : *values_) value_ = std::clamp(value_, 0.1f, 3.0f);
}

float HumanBossAIComponent::GetReactionCost(HumanBossActionType type_) const
{
    if (type_ == HumanBossActionType::BACK_DODGE || type_ == HumanBossActionType::SIDE_DODGE) return mindSettings.dodgeCost;
    if (type_ == HumanBossActionType::GUARD) return mindSettings.guardCost;
    if (type_ == HumanBossActionType::JUST_GUARD) return mindSettings.justGuardCost;
    return 0.0f;
}

bool HumanBossAIComponent::IsReactionReady() const
{
    return playerAttackDetected && playerObservation.attackPhase == ObservedAttackPhase::STARTUP &&
        reactionDelayRemaining <= 0.0f && reactionTimer <= 0.0f && attackSerial != reactedSerial;
}

// 意図の選択は短時間固定し、具体Actionとその終了は既存BehaviorGraph/Skillへ任せる。
void HumanBossAIComponent::UpdateMind(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::UpdateMind");
    const float delta_ = (std::max)(0.0f, deltaTime_);
    intentLockTimer = (std::max)(0.0f, intentLockTimer - delta_);
    whiffTimer = (std::max)(0.0f, whiffTimer - delta_);
    airHeavyPressureTimer = (std::max)(0.0f, airHeavyPressureTimer - delta_);
    airHeavyMemoryTimer = (std::max)(0.0f, airHeavyMemoryTimer - delta_);
    if (airHeavyMemoryTimer <= 0.0f) airHeavyAttackChain = 0;
    if (playerObservation.airborne && playerObservation.heavyAttack && playerObservation.attackPhase != ObservedAttackPhase::NONE &&
        airHeavyAttackSerial != playerObservation.serial)
    {
        // 空中強攻撃を一回だけ数え、回避を挟んでも連続した攻撃パターンを見失わない。
        airHeavyAttackSerial = playerObservation.serial;
        airHeavyAttackChain = (std::min)(airHeavyAttackChain + 1, 8);
        airHeavyMemoryTimer = mindSettings.airHeavyMemoryWindow;
        airHeavyPressureTimer = mindSettings.airHeavyPressureWindow;
        if (currentAction < 0) intentLockTimer = 0.0f;
    }
    feintTimer = (std::max)(0.0f, feintTimer - delta_);
    if (distance <= preferredCombatDistance + 0.25f) baitTimer = (std::max)(0.0f, baitTimer - delta_);
    counterWindowTimer = (std::max)(0.0f, counterWindowTimer - delta_);
    if (currentAction < 0 && counterWindowTimer <= 0.0f) { counterPending = false; lightCounterPending = false; }
    bossHitChainTimer = (std::max)(0.0f, bossHitChainTimer - delta_);
    if (bossHitChainTimer <= 0.0f) bossHitStreak = 0;
    const float repeats_ = std::clamp(static_cast<float>(sameActionRepeatCount - 1) / 4.0f, 0.0f, 1.0f);
    const float airDelay_ = playerObservation.airborne && playerObservation.heavyAttack ? mindSettings.airHeavyReactionDelayMultiplier : 1.0f;
    const float delay_ = (std::max)(0.06f, mindSettings.reactionDelay * mindSettings.phaseDelay[phase] * airDelay_ * (1.0f - repeats_ * mindSettings.repeatPredictionDiscount));
    reactionDelayRemaining = (std::max)(0.0f, delay_ - playerObservation.attackAge);
    if (!debugFreezeBudget && currentAction < 0 && hitTimer <= 0.0f)
        reactionBudget = (std::min)(mindSettings.reactionBudgetMax, reactionBudget + mindSettings.reactionBudgetRecovery * delta_);
    reactionBudget = std::clamp(reactionBudget, 0.0f, mindSettings.reactionBudgetMax);
    if (hasPreviousPlayerPosition && delta_ > 0.0f)
    {
        // Boss自身の接近でPlayerの接近傾向が増えないよう、Playerの実変位を使う。
        const float speed_ = ((playerObservation.position.x - previousPlayerPosition.x) * toPlayer.x +
            (playerObservation.position.z - previousPlayerPosition.z) * toPlayer.z) / delta_;
        const float rate_ = std::clamp(delta_ * 2.0f, 0.0f, 1.0f);
        approachTendency = std::lerp(approachTendency, speed_ < -0.2f ? 1.0f : 0.0f, rate_);
        retreatTendency = std::lerp(retreatTendency, speed_ > 0.2f ? 1.0f : 0.0f, rate_);
    }
    previousPlayerPosition = playerObservation.position;
    hasPreviousPlayerPosition = true;
    const auto owner_ = GetGameObject();
    if (owner_ && owner_->manager)
    {
        const auto observation_ = owner_->GetManager().GetCombatObservationSystem();
        const auto hits_ = observation_ ? observation_->GetObservedHitCount(owner_) : 0;
        if (hits_ > bossHitsObserved) { aggression = (std::min)(1.0f, aggression + 0.05f); confidence = (std::min)(1.0f, confidence + 0.05f); }
        bossHitsObserved = hits_;
    }
    if (mindPlayerSerial != playerObservation.serial)
    {
        if (playerObservation.dodging && currentAction >= 0 && state == HumanBossState::OFFENSE && phase >= 1 &&
            std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < mindSettings.punishChance) comboDelayPending = true;
        if (playerObservation.attackPhase == ObservedAttackPhase::STARTUP) attackWasThreatening = false;
        mindPlayerSerial = playerObservation.serial;
    }
    if (playerAttackDetected) attackWasThreatening = true;
    if (playerObservation.attackPhase == ObservedAttackPhase::RECOVERY && attackWasThreatening && !playerObservation.attackMadeContact && whiffSerial != playerObservation.serial)
    {
        whiffSerial = playerObservation.serial;
        whiffTimer = mindSettings.whiffWindow;
    }
    if (!playerObservation.guarding) guardProbeCount = 0;
    // 一つの新しい攻撃に対して一度だけ、反応時間の満了時に判断の機会を設ける。
    if (currentAction < 0 && IsReactionReady() && reactionDecisionSerial != playerObservation.serial)
    { reactionDecisionSerial = playerObservation.serial; intentLockTimer = 0.0f; }
    if (currentAction < 0 && hitTimer <= 0.0f && airHeavyPressureTimer > 0.0f && !debugForceIntent && !IsReactionReady())
    {
        // 高さ制約を満たす攻撃だけ着地後に選び、それまでは安全な距離へ追跡する。
        const auto executor_ = executor.lock();
        const auto target_ = target.lock();
        // Guard成功後の既存反撃窓は、着地へ届く場合に優先して消費する。
        const bool canCounter_ = (counterPending && IsAvailable(static_cast<std::size_t>(HumanBossActionType::COUNTER_ATTACK))) ||
            (lightCounterPending && IsAvailable(static_cast<std::size_t>(HumanBossActionType::LIGHT_COUNTER)));
        if (canCounter_)
        {
            waitTimer = 0.0f;
            repositionTimer = 0.0f;
            intent = HumanBossIntent::COUNTER_ATTACK;
            intentLockTimer = 0.0f;
            intentReason = "Air heavy pattern: guard counter on landing";
            return;
        }
        const auto punishIndex_ = static_cast<std::size_t>(HumanBossActionType::WHIFF_PUNISH);
        const bool canPunish_ = !playerObservation.airborne && playerObservation.attackPhase != ObservedAttackPhase::ACTIVE && executor_ &&
            distance <= actions[punishIndex_].maxDistance && executor_->CanStart(actions[punishIndex_].skill, HumanBossDefense::NONE, target_);
        whiffTimer = canPunish_ ? (std::max)(whiffTimer, mindSettings.whiffWindow) : whiffTimer;
        waitTimer = 0.0f;
        repositionTimer = 0.0f;
        baitTimer = 0.0f;
        intent = canPunish_ ? HumanBossIntent::PUNISH : HumanBossIntent::CHASE;
        intentLockTimer = 0.0f;
        intentReason = canPunish_ ? "Air heavy pattern: punish landing" : "Air heavy pattern: chase landing range";
        return;
    }
    if (currentAction >= 0 || debugForceIntent || intentLockTimer > 0.0f || (intent == HumanBossIntent::BAIT && baitTimer > 0.0f && !IsReactionReady())) return;
    std::vector<HumanBossIntent> candidates_{ HumanBossIntent::OBSERVE };
    std::vector<float> weights_{ waitTimer > 0.0f ? 2.0f : 0.5f };
    const auto add_ = [&candidates_, &weights_](HumanBossIntent intent_, float weight_) { if (weight_ > 0.0f) { candidates_.push_back(intent_); weights_.push_back(weight_); } };
    add_(HumanBossIntent::COUNTER_ATTACK, counterPending || lightCounterPending ? 20.0f : 0.0f);
    add_(HumanBossIntent::DEFEND, IsReactionReady() && reactionBudget >= (std::min)(mindSettings.guardCost, mindSettings.dodgeCost) ? 4.0f + playerObservation.weakAttackRate * repeats_ * 3.0f : 0.0f);
    add_(HumanBossIntent::REPOSITION, repositionTimer > 0.0f ? 6.0f : 0.0f);
    add_(HumanBossIntent::DISENGAGE, reactionBudget < mindSettings.dodgeCost && playerObservation.attackFrequency > 0.6f && distance < preferredCombatDistance ? 3.0f : 0.0f);
    const float attackDecisionDistance_ = GetAttackDecisionDistance();
    add_(HumanBossIntent::CHASE, distance > attackDecisionDistance_ ? 8.0f + retreatTendency * 2.0f : 0.0f);
    add_(HumanBossIntent::PUNISH, whiffTimer > 0.0f && distance <= attackDecisionDistance_ ? mindSettings.punishChance * (playerObservation.heavyAttack ? 5.0f : 4.0f) : 0.0f);
    if (waitTimer <= 0.0f && repositionTimer <= 0.0f && distance <= attackDecisionDistance_)
    {
        add_(HumanBossIntent::PRESSURE, (1.0f + aggression) * mindSettings.phaseAggression[phase]);
        add_(HumanBossIntent::BREAK_GUARD, phase >= 1 && guardProbeCount > 0 && playerObservation.guardHoldTime >= mindSettings.guardHoldThreshold && guardFrequency >= mindSettings.guardReadThreshold ? 1.0f + guardFrequency * mindSettings.guardReadBoost : 0.0f);
        add_(HumanBossIntent::PUNISH_DODGE, phase >= 1 && (dodgeFrequency >= mindSettings.dodgeReadThreshold || playerObservation.consecutiveDodges >= 2) ? 1.0f + dodgeFrequency * mindSettings.dodgeReadBoost : 0.0f);
        add_(HumanBossIntent::BAIT, phase >= 1 && feintTimer <= 0.0f ? mindSettings.baitChance * 3.0f : 0.0f);
    }
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < mindSettings.readErrorChance) intent = HumanBossIntent::OBSERVE;
    else intent = candidates_[std::discrete_distribution<std::size_t>(weights_.begin(), weights_.end())(random)];
    if (intent == HumanBossIntent::BAIT) baitTimer = mindSettings.baitWait;
    intentLockTimer = mindSettings.decisionLockTime;
    intentReason = std::string(GetHumanBossIntentName(intent)) + " / weak=" + std::to_string(playerObservation.weakAttackRate) +
        " guard=" + std::to_string(guardFrequency) + " dodge=" + std::to_string(dodgeFrequency) + " distance=" + std::to_string(distance);
}

float HumanBossAIComponent::GetIntentActionModifier(std::size_t index_) const
{
    const auto& definition_ = actions[index_];
    const auto type_ = definition_.type;
    if (definition_.category == HumanBossState::REACTION)
    {
        if (intent != HumanBossIntent::DEFEND || !IsReactionReady() || reactionBudget < GetReactionCost(type_)) return 0.0f;
        const float delayMultiplier_ = type_ == HumanBossActionType::GUARD ? mindSettings.guardDelayMultiplier :
            type_ == HumanBossActionType::JUST_GUARD ? mindSettings.justGuardDelayMultiplier : mindSettings.dodgeDelayMultiplier;
        const float repeatRead_ = std::clamp(static_cast<float>(sameActionRepeatCount - 1) / 4.0f, 0.0f, 1.0f);
        const float airDelay_ = playerObservation.airborne && playerObservation.heavyAttack ? mindSettings.airHeavyReactionDelayMultiplier : 1.0f;
        const float requiredDelay_ = (std::max)(0.06f, mindSettings.reactionDelay * mindSettings.phaseDelay[phase] * airDelay_ * delayMultiplier_ * (1.0f - repeatRead_ * mindSettings.repeatPredictionDiscount));
        if (playerObservation.attackAge < requiredDelay_) return 0.0f;
        float weight_ = static_cast<int>(index_) == lastReaction ? std::pow(mindSettings.sameReactionPenalty, static_cast<float>((std::max)(1, reactionStreak))) : 1.0f;
        if (type_ == HumanBossActionType::JUST_GUARD)
        {
            const float reads_ = sameActionRepeatCount >= 3 ? (playerObservation.weakAttackRate + playerObservation.heavyAttackRate) * confidence : 0.0f;
            weight_ *= 1.0f + reads_ * mindSettings.weakReadBoost;
        }
        return weight_;
    }
    if (type_ == HumanBossActionType::COUNTER_ATTACK) return intent == HumanBossIntent::COUNTER_ATTACK && counterPending ? 1.0f : 0.0f;
    if (type_ == HumanBossActionType::LIGHT_COUNTER) return intent == HumanBossIntent::COUNTER_ATTACK && lightCounterPending ? 1.0f : 0.0f;
    if (type_ == HumanBossActionType::WHIFF_PUNISH) return intent == HumanBossIntent::PUNISH && whiffTimer > 0.0f ? 2.0f : 0.0f;
    if (type_ == HumanBossActionType::DELAY_ATTACK) return intent == HumanBossIntent::PUNISH_DODGE ? 1.0f + dodgeFrequency * mindSettings.dodgeReadBoost : 0.0f;
    if (type_ == HumanBossActionType::GUARD_BREAK) return intent == HumanBossIntent::BREAK_GUARD && guardProbeCount > 0 && playerObservation.guarding ? 1.0f + guardFrequency * mindSettings.guardReadBoost : 0.0f;
    if (type_ == HumanBossActionType::ATTACK_FEINT) return intent == HumanBossIntent::BAIT && baitTimer <= 0.0f && feintTimer <= 0.0f ? mindSettings.feintChance * (lastAction == static_cast<int>(index_) ? mindSettings.feintRecentPenalty : 1.0f) : 0.0f;
    if (definition_.category == HumanBossState::SHADOW)
    {
        if (type_ >= HumanBossActionType::SHADOW_SLASH)
        {
            if (type_ == HumanBossActionType::SHADOW_DOMAIN)
                return intent == HumanBossIntent::BAIT || intent == HumanBossIntent::DISENGAGE || playerObservation.guarding ? 0.6f : 0.0f;
            if (type_ == HumanBossActionType::SHADOW_SLASH)
                return intent == HumanBossIntent::CHASE || intent == HumanBossIntent::PUNISH || intent == HumanBossIntent::PUNISH_DODGE ? 1.0f : 0.0f;
            return intent == HumanBossIntent::CHASE || intent == HumanBossIntent::PUNISH || intent == HumanBossIntent::REPOSITION ? 0.6f : 0.0f;
        }
        if (type_ == HumanBossActionType::SHADOW_SHIFT_APPROACH) return intent == HumanBossIntent::CHASE ? 1.0f + retreatTendency : 0.0f;
        if (type_ == HumanBossActionType::SHADOW_SHIFT_BACK) return intent == HumanBossIntent::DISENGAGE || intent == HumanBossIntent::REPOSITION ? 1.0f : 0.0f;
        if (type_ == HumanBossActionType::SHADOW_SHIFT_SIDE)
            return intent == HumanBossIntent::REPOSITION || intent == HumanBossIntent::DISENGAGE || intent == HumanBossIntent::BAIT ? 1.0f : 0.0f;
        return intent == HumanBossIntent::BAIT || (intent == HumanBossIntent::PUNISH && !playerObservation.airborne) ? 0.7f : 0.0f;
    }
    if (definition_.category == HumanBossState::OFFENSE)
        return (intent == HumanBossIntent::CHASE && airHeavyPressureTimer > 0.0f && !playerObservation.airborne) || intent == HumanBossIntent::PRESSURE || intent == HumanBossIntent::PUNISH || intent == HumanBossIntent::BREAK_GUARD || intent == HumanBossIntent::PUNISH_DODGE || (intent == HumanBossIntent::BAIT && baitTimer <= 0.0f) ? 1.0f : 0.0f;
    return 1.0f;
}

void HumanBossAIComponent::DrawMindGui()
{
    constexpr const char* REACTION_NAMES[] = { "BACK_DODGE", "SIDE_DODGE", "GUARD", "JUST_GUARD" };
    const char* reactionName_ = lastReaction >= 2 && lastReaction <= 5 ? REACTION_NAMES[lastReaction - 2] : "NONE";
    ImGui::Text("Intent: %s / Last Reaction: %s x%d", GetHumanBossIntentName(intent), reactionName_, reactionStreak);
    ImGui::Text("Attack %.2f / Weak %.2f / Heavy %.2f", playerObservation.attackFrequency, playerObservation.weakAttackRate, playerObservation.heavyAttackRate);
    ImGui::Text("Guard %.2f / Dodge %.2f / Same %d / Dodge streak %d", guardFrequency, dodgeFrequency, sameActionRepeatCount, playerObservation.consecutiveDodges);
    ImGui::Text("Air heavy pattern: %d / Pressure %.2f / Memory %.2f", airHeavyAttackChain, airHeavyPressureTimer, airHeavyMemoryTimer);
    ImGui::Text("Budget %.2f / Delay %.2f / Lock %.2f", reactionBudget, reactionDelayRemaining, intentLockTimer);
    ImGui::Text("Approach %.2f / Retreat %.2f / Whiff %.2f / Feint %.2f", approachTendency, retreatTendency, whiffTimer, feintTimer);
    std::array<int, 5> directions_{};
    for (const auto& action_ : playerObservation.history)
        if (action_.dodge) ++directions_[std::clamp(static_cast<int>(action_.dodgeDirection), 0, 4)];
    ImGui::Text("Dodge directions L:%d R:%d B:%d F:%d", directions_[1], directions_[2], directions_[3], directions_[4]);
    ImGui::TextWrapped("Reason: %s", intentReason.c_str());
    ImGui::SliderFloat("Aggression", &aggression, 0.0f, 1.0f);
    ImGui::SliderFloat("Confidence", &confidence, 0.0f, 1.0f);
    ImGui::SliderFloat("Reaction Budget Debug", &reactionBudget, 0.0f, mindSettings.reactionBudgetMax);
    ImGui::Checkbox("Freeze Budget Debug", &debugFreezeBudget);
    ImGui::Checkbox("Force Intent Debug", &debugForceIntent);
    ImGui::Checkbox("Force Phase Debug", &debugForcePhase);
    if (debugForcePhase) ImGui::SliderInt("Debug Phase Index", &debugPhase, 0, 2);
    if (debugForceIntent)
    {
        const char* NAMES[] = { "NONE", "OBSERVE", "PRESSURE", "BAIT", "PUNISH", "DEFEND", "REPOSITION", "DISENGAGE", "CHASE", "BREAK_GUARD", "PUNISH_DODGE", "COUNTER_ATTACK" };
        int value_ = static_cast<int>(intent);
        if (ImGui::Combo("Forced Intent", &value_, NAMES, 12)) intent = static_cast<HumanBossIntent>(value_);
    }
    if (ImGui::TreeNode("Mind Settings")) { mindSettings.Gui(); ImGui::TreePop(); }
    if (ImGui::TreeNode("Manager Player Observation Debug"))
    {
        const auto owner_ = GetGameObject();
        if (owner_ && owner_->manager)
            if (const auto observation_ = owner_->GetManager().GetCombatObservationSystem()) observation_->DrawPlayerActionObservationDebug(playerObjectName);
        ImGui::TreePop();
    }
}
