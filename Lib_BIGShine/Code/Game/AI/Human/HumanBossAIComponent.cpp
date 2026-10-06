#include "PrecompiledHeader.h"
#include "HumanBossAIComponent.h"
#include "HumanBossActionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"

namespace
{
    constexpr const char* STATE_NAMES[] = { "NEUTRAL", "REACTION", "OFFENSE", "REPOSITION", "SHADOW", "DRAGON_SHADOW", "SPECIAL" };
    constexpr const char* ACTION_NAMES[] =
    {
        "NORMAL_ATTACK_A", "NORMAL_ATTACK_B", "BACK_DODGE", "SIDE_DODGE", "GUARD", "JUST_GUARD",
        "COUNTER_ATTACK", "DELAY_ATTACK", "GUARD_BREAK", "SHADOW_SHIFT_BACK", "SHADOW_SHIFT_SIDE",
        "SHADOW_SHIFT_BEHIND", "SHADOW_SHIFT_APPROACH", "DRAGON_HEAD_BREATH", "DRAGON_BITE",
        "DRAGON_CLAW", "DRAGON_TAIL", "DRAGON_WING", "SPECIAL_ATTACK", "SPECIAL_COUNTER"
    };
    // 不正な保存値は配列参照に使わず、未選択として表示する。
    const char* ActionName(int index_)
    {
        return index_ >= 0 && index_ < static_cast<int>(HumanBossActionType::COUNT) ? ACTION_NAMES[index_] : "NONE";
    }
    bool IsDodge(HumanBossActionType type_)
    {
        return type_ == HumanBossActionType::BACK_DODGE || type_ == HumanBossActionType::SIDE_DODGE;
    }
}

HumanBossAIComponent::HumanBossAIComponent()
{
    // AIはMovementより先に予約する。Animation/Eventは既存の更新順を維持する。
    SetUpdatePriority(-20);
    for (int index_ = 0; index_ < static_cast<int>(HumanBossActionType::COUNT); ++index_)
    {
        HumanBossActionDefinition definition_;
        definition_.type = static_cast<HumanBossActionType>(index_);
        definition_.skill.name = ACTION_NAMES[index_];
        definition_.enabled = { false, false, false };
        if (index_ >= 2 && index_ <= 5) definition_.category = HumanBossState::REACTION;
        if (index_ >= 9 && index_ <= 12) definition_.category = HumanBossState::SHADOW;
        if (index_ >= 13 && index_ <= 17) definition_.category = HumanBossState::DRAGON_SHADOW;
        if (index_ >= 18) definition_.category = HumanBossState::SPECIAL;
        actions.push_back(definition_);
    }
    const char* ANIMATIONS[] = { "SS_Attack1", "SS_Attack2", "SS_Dodge", "SS_Dodge", "SS_Guard", "Parry_L_Seq", "HumanBossCounter" };
    const float WEIGHTS[] = { 1.0f, 1.0f, 40.0f, 15.0f, 20.0f, 10.0f, 1.0f };
    for (int index_ = 0; index_ < 7; ++index_)
    {
        auto& definition_ = actions[index_];
        definition_.enabled = { true, true, true };
        definition_.weight.fill(WEIGHTS[index_]);
        definition_.cooldown.fill(index_ == 5 ? 2.5f : 1.2f);
        SkillPhase phase_;
        phase_.animationState = ANIMATIONS[index_];
        phase_.endCondition = SkillPhaseEnd::ANIMATION_END;
        phase_.maximumDuration = 5.0f;
        if (index_ == 4)
        {
            phase_.endCondition = SkillPhaseEnd::DURATION;
            phase_.duration = 0.7f;
            phase_.enterAction = "EnableGuardCollider";
            phase_.exitAction = "DisableGuardCollider";
        }
        if (index_ == 2 || index_ == 3)
        {
            phase_.endCondition = SkillPhaseEnd::DURATION;
            phase_.duration = 0.75f;
        }
        definition_.skill.phases.push_back(phase_);
    }
    actions[0].skill.category = SkillCategory::ATTACK;
    actions[1].skill.category = SkillCategory::ATTACK;
    actions[5].enabled = { false, true, true };
    actions[6].enabled = { false, true, true };
    actions[6].skill.category = SkillCategory::COUNTER;
    cooldowns.resize(actions.size(), 0.0f);
}

const std::string& HumanBossAIComponent::GetName() const
{
    static const std::string NAME = "HumanBossAIComponent";
    return NAME;
}

void HumanBossAIComponent::Start()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Start");
    NormalizeSettings();
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const auto executor_ = owner_->GetOrAddComponent<HumanBossActionComponent>(false);
    executor = executor_;
    executor_->RegisterActions();
    owner_->GetOrAddComponent<BlackboardComponent>();
    cooldowns.assign(actions.size(), 0.0f);
    runtimeStarted = true;
    RegisterActions();
    EnterNeutral();
}

void HumanBossAIComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<HumanBossAIComponent> weak_ = owner_->GetComponent<HumanBossAIComponent>();
    // HitReactionのダメージ処理を維持し、ひるみ中はNeutralがAnimationを上書きしない。
    owner_->RegisterAction("HumanBoss", "HumanBossHit", [weak_]()
    {
        const auto self_ = weak_.lock();
        const auto actor_ = self_ ? self_->GetGameObject() : nullptr;
        if (!actor_) return;
        bool reacted_ = true;
        if (const auto blackboard_ = actor_->GetComponent<BlackboardComponent>())
            blackboard_->GetBool("Hit", "ImpactReacted", reacted_);
        if (!reacted_) return;
        self_->Stop();
        self_->hitTimer = self_->hitRecoveryDuration;
        if (const auto executor_ = self_->executor.lock())
            executor_->SetLocomotion(self_->hitState, { 0.0f, 0.0f, 0.0f }, self_->toPlayer, { 0.0f, 0.0f });
    });
    for (int index_ = 0; index_ < static_cast<int>(HumanBossActionType::COUNT); ++index_)
    {
        owner_->RegisterAction("HumanBoss", std::string("HumanBoss_") + ACTION_NAMES[index_], [weak_, index_]()
        {
            if (const auto self_ = weak_.lock()) self_->RequestAction(static_cast<HumanBossActionType>(index_));
        });
    }
}

void HumanBossAIComponent::EnterNeutral()
{
    state = HumanBossState::NEUTRAL;
    currentAction = -1;
    counterPending = false;
    waitTimer = std::uniform_real_distribution<float>(waitMin, waitMax)(random);
    strafeDirection = std::uniform_int_distribution<int>(0, 1)(random) == 0 ? -1.0f : 1.0f;
}

void HumanBossAIComponent::Stop()
{
    if (const auto executor_ = executor.lock()) executor_->Stop(idleState);
    repositionTimer = 0.0f;
    EnterNeutral();
}

bool HumanBossAIComponent::ObservePlayer()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::ObservePlayer");
    playerAttackDetected = false;
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !owner_->transform) return false;
    auto player_ = target.lock();
    if (!player_ || player_->GetName() != playerObjectName)
    {
        player_ = owner_->GetManager().Find(playerObjectName);
        target = player_;
    }
    if (!player_ || player_ == owner_ || !player_->transform) { reactionReason = "Target missing"; return false; }
    if (const auto playerHealth_ = player_->GetComponent<HealthComponent>())
        if (playerHealth_->IsDead()) { reactionReason = "Player dead"; return false; }
    const auto ownerPosition_ = owner_->transform->GetWorldPosition();
    const auto playerPosition_ = player_->transform->GetWorldPosition();
    toPlayer = { playerPosition_.x - ownerPosition_.x, 0.0f, playerPosition_.z - ownerPosition_.z };
    distance = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    if (distance > 0.0001f) { toPlayer.x /= distance; toPlayer.z /= distance; }
    else toPlayer = { 0.0f, 0.0f, 1.0f };
    const auto controller_ = player_->GetComponent<PlayerControllerComponent>();
    const auto animation_ = player_->GetComponent<AnimationControllerComponent>();
    if (!controller_ || !animation_) { reactionReason = "Player controller/animation missing"; return true; }
    const auto action_ = controller_->GetCurrentAction();
    const auto group_ = GetPlayerActionGroup(action_);
    const auto currentState_ = animation_->GetCurrentState();
    const std::string animationName_ = currentState_ ? currentState_->name : std::string();
    const float time_ = animation_->GetAnimatorTime();
    const bool changed_ = observedPlayerAction != static_cast<int>(action_) ||
        observedCombo != controller_->GetComboIndex() || observedAnimation != animationName_ ||
        (group_ == PLAYER_ACTION_GROUP::ATTACK && time_ + 0.001f < previousPlayerAnimationTime);
    if (changed_)
    {
        const std::string name_ = GetPlayerActionName(action_);
        sameActionRepeatCount = recentPlayerAction == name_ ? sameActionRepeatCount + 1 : 1;
        recentPlayerAction = name_;
        observedPlayerAction = static_cast<int>(action_);
        observedCombo = controller_->GetComboIndex();
        observedAnimation = animationName_;
        if (group_ == PLAYER_ACTION_GROUP::ATTACK) ++attackSerial;
        // 周回する待機Animationで履歴を埋めず、戦闘行動だけを16件記録する。
        if (group_ == PLAYER_ACTION_GROUP::ATTACK || group_ == PLAYER_ACTION_GROUP::GUARD || group_ == PLAYER_ACTION_GROUP::DODGE)
        {
            playerHistory.push_back(static_cast<int>(group_));
            if (playerHistory.size() > 16) playerHistory.erase(playerHistory.begin());
            guardFrequency = static_cast<float>(std::count(playerHistory.begin(), playerHistory.end(), static_cast<int>(PLAYER_ACTION_GROUP::GUARD))) / playerHistory.size();
            dodgeFrequency = static_cast<float>(std::count(playerHistory.begin(), playerHistory.end(), static_cast<int>(PLAYER_ACTION_GROUP::DODGE))) / playerHistory.size();
        }
    }
    previousPlayerAnimationTime = time_;
    if (group_ != PLAYER_ACTION_GROUP::ATTACK || !currentState_) { reactionReason = "No player attack"; return true; }
    float reach_ = playerAttackReach;
    float halfAngle_ = playerAttackHalfAngle;
    for (const auto& threat_ : threats)
        if (threat_.animationState == animationName_) { reach_ = threat_.reach; halfAngle_ = threat_.halfAngle; break; }
    const float yaw_ = player_->transform->GetWorldRotation().y * DirectX::XM_PI / 180.0f;
    const float facingDot_ = -(std::sin(yaw_) * toPlayer.x + std::cos(yaw_) * toPlayer.z);
    if (distance > reach_ || std::abs(ownerPosition_.y - playerPosition_.y) > threatHeightTolerance ||
        facingDot_ < std::cos(halfAngle_ * DirectX::XM_PI / 180.0f))
    { reactionReason = "Attack cannot reach boss"; return true; }
    // 攻撃Template/HitBox受付イベントの予兆だけを見る。遠距離の空振りには反応しない。
    incomingTime = (std::numeric_limits<float>::max)();
    for (const auto& event_ : currentState_->events)
    {
        const bool attackEvent_ = event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE ||
            event_.startAction.find("EnableHitBox") != std::string::npos;
        if (!attackEvent_) continue;
        float start_ = event_.startTime;
        float end_ = event_.endTime;
        if (const auto template_ = AttackTemplateManager::Instance().FindTemplate(event_.attackTemplateId))
        {
            if (template_->weaponHitBox.enabled)
            {
                start_ += template_->weaponHitBox.activeStartTime;
                end_ = (std::min)(end_, event_.startTime + template_->weaponHitBox.activeEndTime);
            }
        }
        if (time_ > end_ || start_ - time_ > reactionLookAhead) continue;
        incomingTime = (std::min)(incomingTime, start_ - time_);
        playerAttackDetected = true;
    }
    reactionReason = playerAttackDetected ? "Reachable attack window" : "Outside attack window";
    return true;
}

bool HumanBossAIComponent::IsAvailable(std::size_t index_) const
{
    if (index_ >= actions.size() || index_ >= cooldowns.size()) return false;
    const auto& definition_ = actions[index_];
    if (!definition_.enabled[phase] || cooldowns[index_] > 0.0f ||
        distance < definition_.minDistance || distance > definition_.maxDistance ||
        static_cast<int>(index_) == lastAction) return false;
    if (definition_.category == HumanBossState::REACTION && static_cast<int>(index_) == lastReaction) return false;
    if (definition_.category == HumanBossState::OFFENSE && definition_.type != HumanBossActionType::COUNTER_ATTACK &&
        static_cast<int>(index_) == lastOffense) return false;
    if (IsDodge(definition_.type) && dodgeTimer > 0.0f) return false;
    if (definition_.category == HumanBossState::OFFENSE && definition_.type != HumanBossActionType::COUNTER_ATTACK && attackTimer > 0.0f) return false;
    if (definition_.category == HumanBossState::SHADOW && !shadowEnabled) return false;
    if (definition_.category == HumanBossState::DRAGON_SHADOW && !dragonShadowEnabled) return false;
    if (definition_.category == HumanBossState::SPECIAL && !specialEnabled) return false;
    const HumanBossDefense defense_ = IsDodge(definition_.type) ? HumanBossDefense::DODGE : definition_.type == HumanBossActionType::GUARD ? HumanBossDefense::GUARD : definition_.type == HumanBossActionType::JUST_GUARD ? HumanBossDefense::JUST_GUARD : HumanBossDefense::NONE;
    const auto executor_ = executor.lock();
    return executor_ && executor_->CanStart(definition_.skill, defense_);
}

bool HumanBossAIComponent::BeginAction(std::size_t index_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::BeginAction");
    if (!IsAvailable(index_)) return false;
    const auto& definition_ = actions[index_];
    const auto executor_ = executor.lock();
    DirectX::XMFLOAT3 velocity_{ 0.0f, 0.0f, 0.0f };
    if (definition_.type == HumanBossActionType::BACK_DODGE) velocity_ = { -toPlayer.x * dodgeSpeed, 0.0f, -toPlayer.z * dodgeSpeed };
    if (definition_.type == HumanBossActionType::SIDE_DODGE) velocity_ = { toPlayer.z * strafeDirection * dodgeSpeed, 0.0f, -toPlayer.x * strafeDirection * dodgeSpeed };
    const HumanBossDefense defense_ = IsDodge(definition_.type) ? HumanBossDefense::DODGE : definition_.type == HumanBossActionType::GUARD ? HumanBossDefense::GUARD : definition_.type == HumanBossActionType::JUST_GUARD ? HumanBossDefense::JUST_GUARD : HumanBossDefense::NONE;
    executor_->FaceTarget(toPlayer);
    if (!executor_->Begin(definition_.skill, defense_, velocity_)) return false;
    currentAction = static_cast<int>(index_);
    lastAction = currentAction;
    lastActionCategory = definition_.category;
    state = definition_.category;
    cooldowns[index_] = definition_.cooldown[phase];
    if (state == HumanBossState::REACTION) { reactionTimer = reactionCooldown * phaseReactionMultiplier[phase]; lastReaction = currentAction; }
    if (IsDodge(definition_.type)) dodgeTimer = dodgeCooldown;
    if (state == HumanBossState::OFFENSE && definition_.type != HumanBossActionType::COUNTER_ATTACK)
    { attackTimer = attackCooldown; lastOffense = currentAction; }
    return true;
}

bool HumanBossAIComponent::RequestAction(HumanBossActionType type_)
{
    const auto executor_ = executor.lock();
    // 外部要求も実行中Actionを上書きせず、特殊演出の終了後に通常AIへ戻す。
    if (type_ == HumanBossActionType::COUNTER_ATTACK || hitTimer > 0.0f || !runtimeStarted || !executor_ || executor_->IsActive() || currentAction >= 0 || !ObservePlayer()) return false;
    const auto owner_ = GetGameObject();
    if (const auto health_ = owner_->GetComponent<HealthComponent>()) if (health_->IsDead()) return false;
    bool locked_ = false;
    if (const auto mode_ = owner_->GetManager().GetGameModeManager())
        mode_->GetGlobalBlackboard().GetBool(GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, locked_);
    if (locked_) return false;
    for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
        if (actions[index_].type == type_) return BeginAction(index_);
    return false;
}

bool HumanBossAIComponent::SelectAction(HumanBossState category_, bool reaction_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::SelectAction");
    std::vector<std::size_t> candidates_;
    std::vector<float> weights_;
    for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
    {
        const auto& definition_ = actions[index_];
        if (definition_.category != category_ || !IsAvailable(index_)) continue;
        // Counterは成功通知専用。将来のGuardBreak/Delayは観察履歴に応じて解禁する。
        if (definition_.type == HumanBossActionType::COUNTER_ATTACK) continue;
        if (definition_.type == HumanBossActionType::GUARD_BREAK && guardFrequency < 0.3f) continue;
        if (definition_.type == HumanBossActionType::DELAY_ATTACK && dodgeFrequency < 0.3f) continue;
        if (definition_.type == HumanBossActionType::JUST_GUARD && (incomingTime > justGuardLead || incomingTime < -justGuardLead)) continue;
        if (definition_.weight[phase] <= 0.0f) continue;
        candidates_.push_back(index_);
        weights_.push_back(definition_.weight[phase]);
    }
    if (reaction_) weights_.push_back(noneReactionWeight);
    if (weights_.empty() || std::all_of(weights_.begin(), weights_.end(), [](float weight_) { return weight_ <= 0.0f; })) return false;
    const auto selected_ = std::discrete_distribution<std::size_t>(weights_.begin(), weights_.end())(random);
    if (reaction_)
    {
        // NONEもその攻撃に対する一度の判断として記録し、同じ窓で抽選し直さない。
        reactedSerial = attackSerial;
        reactionTimer = reactionCooldown * phaseReactionMultiplier[phase];
        if (selected_ >= candidates_.size()) { reactionReason = "NONE selected"; return true; }
    }
    return BeginAction(candidates_[selected_]);
}

void HumanBossAIComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Update");
    const auto owner_ = GetGameObject();
    const auto executor_ = executor.lock();
    if (!runtimeStarted || !owner_ || !executor_) return;
    const float delta_ = (std::max)(deltaTime_, 0.0f);
    const auto health_ = owner_->GetComponent<HealthComponent>();
    bool locked_ = false;
    if (owner_->manager)
        if (const auto mode_ = owner_->GetManager().GetGameModeManager())
            mode_->GetGlobalBlackboard().GetBool(GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, locked_);
    if (locked_ || (health_ && health_->IsDead())) { Stop(); PublishDebug(); return; }
    if (!ObservePlayer()) { Stop(); PublishDebug(); return; }
    if (health_ && health_->GetMaxHealth() > 0.0f)
    {
        const float rate_ = health_->GetCurrentHealth() / health_->GetMaxHealth();
        phase = rate_ <= phase3Hp ? 2 : rate_ <= phase2Hp ? 1 : 0;
    }
    for (auto& cooldown_ : cooldowns) cooldown_ = (std::max)(0.0f, cooldown_ - delta_);
    reactionTimer = (std::max)(0.0f, reactionTimer - delta_);
    dodgeTimer = (std::max)(0.0f, dodgeTimer - delta_);
    attackTimer = (std::max)(0.0f, attackTimer - delta_);
    hitTimer = (std::max)(0.0f, hitTimer - delta_);
    if (hitTimer > 0.0f) { PublishDebug(); return; }
    counterPending = executor_->ConsumeJustGuardSuccess() || counterPending;
    executor_->Tick(delta_);
    if (currentAction >= 0)
    {
        if (executor_->IsActive()) { PublishDebug(); return; }
        const bool completed_ = executor_->GetResult() == SkillExecutionResult::COMPLETED;
        const bool counter_ = completed_ && counterPending;
        const bool reposition_ = completed_ && state == HumanBossState::OFFENSE;
        EnterNeutral();
        if (counter_)
        {
            for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
                if (actions[index_].type == HumanBossActionType::COUNTER_ATTACK && BeginAction(index_)) { PublishDebug(); return; }
        }
        if (reposition_) repositionTimer = repositionTime;
    }
    if (executor_->IsActive()) { PublishDebug(); return; }
    if (playerAttackDetected && reactionTimer <= 0.0f && attackSerial != reactedSerial)
    {
        if (SelectAction(HumanBossState::REACTION, true)) { PublishDebug(); return; }
    }
    waitTimer = (std::max)(0.0f, waitTimer - delta_);
    repositionTimer = (std::max)(0.0f, repositionTimer - delta_);
    if (distance < minCombatDistance) repositionTimer = (std::max)(repositionTimer, repositionTime);
    if (waitTimer <= 0.0f && repositionTimer <= 0.0f && distance >= minCombatDistance && distance <= maxCombatDistance)
    {
        if (SelectAction(HumanBossState::OFFENSE)) { PublishDebug(); return; }
        // 未設定Actionだけの構成でも毎フレーム攻撃抽選を続けない。
        waitTimer = std::uniform_real_distribution<float>(waitMin, waitMax)(random);
    }
    DirectX::XMFLOAT3 velocity_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT2 blend_{ 0.0f, 0.0f };
    state = HumanBossState::NEUTRAL;
    if (distance > preferredCombatDistance + 0.25f) { velocity_ = { toPlayer.x * walkSpeed, 0.0f, toPlayer.z * walkSpeed }; blend_.y = 1.0f; }
    else if (distance < minCombatDistance || repositionTimer > 0.0f)
    {
        state = HumanBossState::REPOSITION;
        if (distance < preferredCombatDistance) { velocity_ = { -toPlayer.x * walkSpeed, 0.0f, -toPlayer.z * walkSpeed }; blend_.y = -1.0f; }
        else { velocity_ = { toPlayer.z * strafeDirection * strafeSpeed, 0.0f, -toPlayer.x * strafeDirection * strafeSpeed }; blend_.x = strafeDirection; }
    }
    else if (waitTimer > (waitMin + waitMax) * 0.25f)
    { velocity_ = { toPlayer.z * strafeDirection * strafeSpeed, 0.0f, -toPlayer.x * strafeDirection * strafeSpeed }; blend_.x = strafeDirection; }
    executor_->SetLocomotion(blend_.x == 0.0f && blend_.y == 0.0f ? idleState : walkState, velocity_, toPlayer, blend_);
    PublishDebug();
}

void HumanBossAIComponent::PublishDebug()
{
    const auto owner_ = GetGameObject();
    const auto blackboard_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!blackboard_) return;
    blackboard_->SetInt("HumanBossAI", "Phase", phase + 1);
    blackboard_->SetString("HumanBossAI", "State", STATE_NAMES[static_cast<int>(state)]);
    blackboard_->SetString("HumanBossAI", "Action", ActionName(currentAction));
    blackboard_->SetFloat("HumanBossAI", "Distance", distance);
    blackboard_->SetBool("HumanBossAI", "PlayerAttackDetected", playerAttackDetected);
    blackboard_->SetString("HumanBossAI", "RecentPlayerAction", recentPlayerAction);
    blackboard_->SetInt("HumanBossAI", "SameActionRepeatCount", sameActionRepeatCount);
    blackboard_->SetFloat("HumanBossAI", "GuardFrequency", guardFrequency);
    blackboard_->SetFloat("HumanBossAI", "DodgeFrequency", dodgeFrequency);
}

void HumanBossAIComponent::NormalizeSettings()
{
    walkSpeed = (std::max)(0.0f, walkSpeed);
    strafeSpeed = (std::max)(0.0f, strafeSpeed);
    dodgeSpeed = (std::max)(0.0f, dodgeSpeed);
    reactionCooldown = (std::max)(0.0f, reactionCooldown);
    dodgeCooldown = (std::max)(0.0f, dodgeCooldown);
    attackCooldown = (std::max)(0.0f, attackCooldown);
    reactionLookAhead = (std::max)(0.0f, reactionLookAhead);
    justGuardLead = (std::max)(0.0f, justGuardLead);
    playerAttackReach = (std::max)(0.0f, playerAttackReach);
    threatHeightTolerance = (std::max)(0.0f, threatHeightTolerance);
    noneReactionWeight = (std::max)(0.01f, noneReactionWeight);
    repositionTime = (std::max)(0.0f, repositionTime);
    hitRecoveryDuration = (std::max)(0.05f, hitRecoveryDuration);
    for (auto& threat_ : threats)
    {
        threat_.reach = (std::max)(0.0f, threat_.reach);
        threat_.halfAngle = std::clamp(threat_.halfAngle, 0.0f, 180.0f);
    }
    minCombatDistance = (std::max)(0.0f, minCombatDistance);
    preferredCombatDistance = (std::max)(minCombatDistance, preferredCombatDistance);
    maxCombatDistance = (std::max)(preferredCombatDistance, maxCombatDistance);
    waitMin = (std::max)(0.05f, waitMin);
    waitMax = (std::max)(waitMin, waitMax);
    phase2Hp = std::clamp(phase2Hp, 0.0f, 1.0f);
    phase3Hp = std::clamp(phase3Hp, 0.0f, phase2Hp);
    playerAttackHalfAngle = std::clamp(playerAttackHalfAngle, 0.0f, 180.0f);
    for (auto& multiplier_ : phaseReactionMultiplier) multiplier_ = (std::max)(0.05f, multiplier_);
    for (auto& definition_ : actions)
    {
        definition_.minDistance = (std::max)(0.0f, definition_.minDistance);
        definition_.maxDistance = (std::max)(definition_.minDistance, definition_.maxDistance);
        for (auto& weight_ : definition_.weight) weight_ = (std::max)(0.0f, weight_);
        for (auto& cooldown_ : definition_.cooldown) cooldown_ = (std::max)(0.0f, cooldown_);
    }
}
void HumanBossAIComponent::Gui()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Gui");
    ImGui::Text("Phase %d / %s", phase + 1, STATE_NAMES[static_cast<int>(state)]);
    ImGui::Text("Action: %s / Last: %s (%s)", ActionName(currentAction), ActionName(lastAction), STATE_NAMES[static_cast<int>(lastActionCategory)]);
    ImGui::Text("Distance %.2f / Reaction %.2f / Attack %.2f / Dodge %.2f", distance, reactionTimer, attackTimer, dodgeTimer);
    float shadowCooldown_ = 0.0f;
    for (std::size_t index_ = 0; index_ < actions.size() && index_ < cooldowns.size(); ++index_)
        if (actions[index_].category == HumanBossState::SHADOW) shadowCooldown_ = (std::max)(shadowCooldown_, cooldowns[index_]);
    ImGui::Text("Shadow Cooldown: %.2f / Hit Recovery: %.2f", shadowCooldown_, hitTimer);
    ImGui::Text("Player attack: %s / %s", playerAttackDetected ? "Detected" : "None", reactionReason.c_str());
    ImGui::Text("Recent: %s x%d / Guard %.2f / Dodge %.2f", recentPlayerAction.c_str(), sameActionRepeatCount, guardFrequency, dodgeFrequency);
    ImGui::InputText("Player Object", &playerObjectName);
    ImGui::InputText("Idle State", &idleState);
    ImGui::InputText("Combat Walk State", &walkState);
    ImGui::InputText("Hit State", &hitState);
    ImGui::DragFloat("Hit Recovery Duration", &hitRecoveryDuration, 0.05f, 0.05f, 5.0f);
    ImGui::DragFloat("Min Combat Distance", &minCombatDistance, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Preferred Combat Distance", &preferredCombatDistance, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Max Combat Distance", &maxCombatDistance, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Wait Min", &waitMin, 0.05f, 0.05f, 10.0f);
    ImGui::DragFloat("Wait Max", &waitMax, 0.05f, 0.05f, 10.0f);
    ImGui::DragFloat("Walk Speed", &walkSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Strafe Speed", &strafeSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Dodge Speed", &dodgeSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Reaction Cooldown", &reactionCooldown, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Shared Dodge Cooldown", &dodgeCooldown, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Shared Attack Cooldown", &attackCooldown, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Reaction Look Ahead", &reactionLookAhead, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Just Guard Lead", &justGuardLead, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Default Player Attack Reach", &playerAttackReach, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Default Attack Half Angle", &playerAttackHalfAngle, 1.0f, 0.0f, 180.0f);
    ImGui::DragFloat("Threat Height Tolerance", &threatHeightTolerance, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("NONE Reaction Weight", &noneReactionWeight, 1.0f, 0.0f, 1000.0f);
    ImGui::DragFloat("Reposition Time", &repositionTime, 0.05f, 0.0f, 10.0f);
    ImGui::SliderFloat("Phase 2 HP", &phase2Hp, 0.0f, 1.0f);
    ImGui::SliderFloat("Phase 3 HP", &phase3Hp, 0.0f, 1.0f);
    ImGui::DragFloat3("Phase Reaction Cooldown Multipliers", phaseReactionMultiplier.data(), 0.05f, 0.05f, 5.0f);
    ImGui::Checkbox("Enable Shadow", &shadowEnabled);
    ImGui::Checkbox("Enable Dragon Shadow", &dragonShadowEnabled);
    ImGui::Checkbox("Enable Special", &specialEnabled);
    if (ImGui::TreeNode("Player Attack Reach Overrides"))
    {
        for (std::size_t index_ = 0; index_ < threats.size(); ++index_)
        {
            ImGui::PushID(static_cast<int>(index_));
            ImGui::InputText("Animation State", &threats[index_].animationState);
            ImGui::DragFloat("Reach", &threats[index_].reach, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Half Angle", &threats[index_].halfAngle, 1.0f, 0.0f, 180.0f);
            ImGui::PopID();
        }
        if (ImGui::Button("Add Reach Override")) threats.push_back({});
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Action Definitions"))
    {
        ImGui::TextDisabled("Phase settings: Enabled / Weight / Cooldown. Edit only while idle.");
        const bool busy_ = currentAction >= 0;
        ImGui::BeginDisabled(busy_);
        for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
        {
            auto& definition_ = actions[index_];
            ImGui::PushID(static_cast<int>(index_));
            if (ImGui::TreeNode(ActionName(static_cast<int>(index_))))
            {
                ImGui::Text("Remaining Cooldown: %.2f", index_ < cooldowns.size() ? cooldowns[index_] : 0.0f);
                int category_ = static_cast<int>(definition_.category);
                if (ImGui::Combo("Category", &category_, STATE_NAMES, IM_ARRAYSIZE(STATE_NAMES))) definition_.category = static_cast<HumanBossState>(category_);
                for (int phase_ = 0; phase_ < 3; ++phase_)
                {
                    ImGui::PushID(phase_);
                    ImGui::Text("PHASE_%d", phase_ + 1);
                    ImGui::Checkbox("Enabled", &definition_.enabled[phase_]);
                    ImGui::DragFloat("Weight", &definition_.weight[phase_], 0.5f, 0.0f, 1000.0f);
                    ImGui::DragFloat("Cooldown", &definition_.cooldown[phase_], 0.05f, 0.0f, 120.0f);
                    ImGui::PopID();
                }
                ImGui::DragFloat("Min Distance", &definition_.minDistance, 0.05f, 0.0f, 1000.0f);
                ImGui::DragFloat("Max Distance", &definition_.maxDistance, 0.05f, 0.0f, 1000.0f);
                ImGui::InputText("Cleanup Action", &definition_.skill.cleanupAction);
                for (std::size_t phaseIndex_ = 0; phaseIndex_ < definition_.skill.phases.size(); ++phaseIndex_)
                {
                    auto& skillPhase_ = definition_.skill.phases[phaseIndex_];
                    ImGui::PushID(static_cast<int>(phaseIndex_) + 10);
                    ImGui::Text("Skill Phase %d", static_cast<int>(phaseIndex_));
                    ImGui::InputText("Animation", &skillPhase_.animationState);
                    ImGui::InputText("Sequence", &skillPhase_.sequence);
                    ImGui::InputText("Enter Action", &skillPhase_.enterAction);
                    ImGui::InputText("Exit Action", &skillPhase_.exitAction);
                    int end_ = static_cast<int>(skillPhase_.endCondition);
                    const char* END_NAMES[] = { "ANIMATION_END", "DURATION", "ACTION_END", "GROUND_CONTACT" };
                    if (ImGui::Combo("End", &end_, END_NAMES, IM_ARRAYSIZE(END_NAMES))) skillPhase_.endCondition = static_cast<SkillPhaseEnd>(end_);
                    ImGui::DragFloat("Duration", &skillPhase_.duration, 0.05f, 0.0f, 120.0f);
                    ImGui::DragFloat("Maximum Duration", &skillPhase_.maximumDuration, 0.05f, 0.05f, 120.0f);
                    ImGui::DragFloat("Blend Duration", &skillPhase_.blendDuration, 0.01f, 0.0f, 5.0f);
                    ImGui::DragFloat("Forward Speed", &skillPhase_.forwardSpeed, 0.1f, -100.0f, 100.0f);
                    ImGui::InputText("Forward Speed Curve", &skillPhase_.forwardSpeedCurveName);
                    ImGui::PopID();
                }
                if (ImGui::Button("Add Skill Phase")) definition_.skill.phases.push_back({});
                if (!definition_.skill.phases.empty())
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove Last Skill Phase")) definition_.skill.phases.pop_back();
                }
                if (!IsAvailable(index_)) ImGui::TextDisabled("Unavailable: cooldown, repeat, distance, or missing assets.");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
    NormalizeSettings();
}

void HumanBossAIComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Serialize");
    json_["PlayerObjectName"] = playerObjectName;
    json_["IdleState"] = idleState;
    json_["WalkState"] = walkState;
    json_["HitState"] = hitState;
    json_["HitRecoveryDuration"] = hitRecoveryDuration;
    json_["MinCombatDistance"] = minCombatDistance;
    json_["PreferredCombatDistance"] = preferredCombatDistance;
    json_["MaxCombatDistance"] = maxCombatDistance;
    json_["WaitMin"] = waitMin;
    json_["WaitMax"] = waitMax;
    json_["WalkSpeed"] = walkSpeed;
    json_["StrafeSpeed"] = strafeSpeed;
    json_["DodgeSpeed"] = dodgeSpeed;
    json_["ReactionCooldown"] = reactionCooldown;
    json_["DodgeCooldown"] = dodgeCooldown;
    json_["AttackCooldown"] = attackCooldown;
    json_["ReactionLookAhead"] = reactionLookAhead;
    json_["JustGuardLead"] = justGuardLead;
    json_["PlayerAttackReach"] = playerAttackReach;
    json_["PlayerAttackHalfAngle"] = playerAttackHalfAngle;
    json_["ThreatHeightTolerance"] = threatHeightTolerance;
    json_["NoneReactionWeight"] = noneReactionWeight;
    json_["RepositionTime"] = repositionTime;
    json_["Phase2Hp"] = phase2Hp;
    json_["Phase3Hp"] = phase3Hp;
    json_["PhaseReactionMultiplier"] = phaseReactionMultiplier;
    json_["ShadowEnabled"] = shadowEnabled;
    json_["DragonShadowEnabled"] = dragonShadowEnabled;
    json_["SpecialEnabled"] = specialEnabled;
    json_["Threats"] = nlohmann::json::array();
    for (const auto& threat_ : threats)
        json_["Threats"].push_back({ { "Animation", threat_.animationState }, { "Reach", threat_.reach }, { "HalfAngle", threat_.halfAngle } });
    json_["Actions"] = nlohmann::json::array();
    for (const auto& definition_ : actions)
    {
        nlohmann::json actionJson_;
        actionJson_["Type"] = static_cast<int>(definition_.type);
        actionJson_["Category"] = static_cast<int>(definition_.category);
        actionJson_["Enabled"] = definition_.enabled;
        actionJson_["Weight"] = definition_.weight;
        actionJson_["Cooldown"] = definition_.cooldown;
        actionJson_["MinDistance"] = definition_.minDistance;
        actionJson_["MaxDistance"] = definition_.maxDistance;
        SerializeSkillDefinition(definition_.skill, actionJson_["Skill"]);
        json_["Actions"].push_back(actionJson_);
    }
}

void HumanBossAIComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Deserialize");
    // 省略項目は既存値を維持し、古いJSONへ新規設定を要求しない。
    playerObjectName = json_.value("PlayerObjectName", playerObjectName);
    idleState = json_.value("IdleState", idleState);
    walkState = json_.value("WalkState", walkState);
    hitState = json_.value("HitState", hitState);
    hitRecoveryDuration = json_.value("HitRecoveryDuration", hitRecoveryDuration);
    minCombatDistance = json_.value("MinCombatDistance", minCombatDistance);
    preferredCombatDistance = json_.value("PreferredCombatDistance", preferredCombatDistance);
    maxCombatDistance = json_.value("MaxCombatDistance", maxCombatDistance);
    waitMin = json_.value("WaitMin", waitMin);
    waitMax = json_.value("WaitMax", waitMax);
    walkSpeed = json_.value("WalkSpeed", walkSpeed);
    strafeSpeed = json_.value("StrafeSpeed", strafeSpeed);
    dodgeSpeed = json_.value("DodgeSpeed", dodgeSpeed);
    reactionCooldown = json_.value("ReactionCooldown", reactionCooldown);
    dodgeCooldown = json_.value("DodgeCooldown", dodgeCooldown);
    attackCooldown = json_.value("AttackCooldown", attackCooldown);
    reactionLookAhead = json_.value("ReactionLookAhead", reactionLookAhead);
    justGuardLead = json_.value("JustGuardLead", justGuardLead);
    playerAttackReach = json_.value("PlayerAttackReach", playerAttackReach);
    playerAttackHalfAngle = json_.value("PlayerAttackHalfAngle", playerAttackHalfAngle);
    threatHeightTolerance = json_.value("ThreatHeightTolerance", threatHeightTolerance);
    noneReactionWeight = json_.value("NoneReactionWeight", noneReactionWeight);
    repositionTime = json_.value("RepositionTime", repositionTime);
    phase2Hp = json_.value("Phase2Hp", phase2Hp);
    phase3Hp = json_.value("Phase3Hp", phase3Hp);
    phaseReactionMultiplier = json_.value("PhaseReactionMultiplier", phaseReactionMultiplier);
    shadowEnabled = json_.value("ShadowEnabled", shadowEnabled);
    dragonShadowEnabled = json_.value("DragonShadowEnabled", dragonShadowEnabled);
    specialEnabled = json_.value("SpecialEnabled", specialEnabled);
    if (json_.contains("Threats") && json_["Threats"].is_array())
    {
        threats.clear();
        for (const auto& threatJson_ : json_["Threats"])
            threats.push_back({ threatJson_.value("Animation", std::string()), threatJson_.value("Reach", playerAttackReach), threatJson_.value("HalfAngle", playerAttackHalfAngle) });
    }
    if (json_.contains("Actions") && json_["Actions"].is_array())
    {
        for (const auto& actionJson_ : json_["Actions"])
        {
            const int type_ = actionJson_.value("Type", -1);
            if (type_ < 0 || type_ >= static_cast<int>(actions.size())) continue;
            auto& definition_ = actions[type_];
            definition_.category = static_cast<HumanBossState>(std::clamp(actionJson_.value("Category", static_cast<int>(definition_.category)), 0, 6));
            definition_.enabled = actionJson_.value("Enabled", definition_.enabled);
            definition_.weight = actionJson_.value("Weight", definition_.weight);
            definition_.cooldown = actionJson_.value("Cooldown", definition_.cooldown);
            definition_.minDistance = actionJson_.value("MinDistance", definition_.minDistance);
            definition_.maxDistance = actionJson_.value("MaxDistance", definition_.maxDistance);
            if (actionJson_.contains("Skill"))
            {
                SkillDefinition skill_;
                std::string error_;
                if (DeserializeSkillDefinition(actionJson_["Skill"], skill_, error_)) definition_.skill = std::move(skill_);
            }
        }
    }
    NormalizeSettings();
    cooldowns.assign(actions.size(), 0.0f);
}
