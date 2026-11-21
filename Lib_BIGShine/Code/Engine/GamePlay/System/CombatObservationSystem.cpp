#include "PrecompiledHeader.h"
#include "CombatObservationSystem.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Game/AI/Human/HumanBossAIComponent.h"
#include "Game/AI/Dragon/DragonAIComponent.h"
#include "Engine/GamePlay/AI/AIControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/System/GuardSystem.h"
#include "Engine/GamePlay/System/JustGuardSystem.h"
#include "Engine/Debug/TracyProfiler.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

void CombatObservationSystem::RemoveNameIndex(GameObject* object_, const std::string& name_)
{
    const auto found_ = objectsByName.find(name_);
    if (found_ == objectsByName.end())
    {
        return;
    }
    auto& list_ = found_->second;
    list_.erase(std::remove_if(list_.begin(), list_.end(), [object_](const auto& weak_)
        {
        const auto value_ = weak_.lock();
        return !value_ || value_.get() == object_;
    }), list_.end());
    if (list_.empty())
    {
        objectsByName.erase(found_);
    }
}

void CombatObservationSystem::RegisterObject(const std::shared_ptr<GameObject>& object_)
{
    BS_PROFILE_SCOPE("CombatObservationSystem::RegisterObject");
    if (!object_)
    {
        return;
    }
    const auto identity_ = object_.get();
    const auto previous_ = objectReferences.find(identity_);
    if (previous_ != objectReferences.end() && previous_->second.object.lock() != object_)
    {
        UnregisterObject(identity_);
    }
    const auto old_ = objectReferences.find(identity_);
    const bool wasPlayer_ = old_ != objectReferences.end() && old_->second.playerLayer;
    const bool isPlayer_ = object_->GetLayer() == LayerType::Player;
    const bool topologyChanged_ = old_ == objectReferences.end() || old_->second.name != object_->GetName() || wasPlayer_ != isPlayer_;
    if (old_ == objectReferences.end() || old_->second.name != object_->GetName())
    {
        if (old_ != objectReferences.end())
        {
            RemoveNameIndex(identity_, old_->second.name);
        }
        objectsByName[object_->GetName()].push_back(object_);
    }
    objectReferences[identity_] = { object_, object_->GetName(), isPlayer_ };
    if (wasPlayer_ != isPlayer_)
    {
        playerLayerObjects.erase(std::remove_if(playerLayerObjects.begin(), playerLayerObjects.end(),
            [identity_](const auto& weak_)
        {
            const auto value_ = weak_.lock();
            return !value_ || value_.get() == identity_;
        }), playerLayerObjects.end());
        if (isPlayer_)
        {
            playerLayerObjects.push_back(object_);
        }
    }
    if (topologyChanged_)
    {
        ++registrationSerial;
    }
    const auto controller_ = object_->GetComponent<PlayerControllerComponent>();
    const auto animation_ = object_->GetComponent<AnimationControllerComponent>();
    const auto health_ = object_->GetComponent<HealthComponent>();
    std::shared_ptr<Component> ai_ = object_->GetComponent<HumanBossAIComponent>();
    if (!ai_)
    {
        ai_ = object_->GetComponent<DragonAIComponent>();
    }
    if (!ai_)
    {
        ai_ = object_->GetComponent<AIControllerComponent>();
    }
    if (controller_ || health_ || ai_)
    {
        auto& actor_ = actors[identity_];
        actor_.object = object_;
        actor_.animation = animation_;
        actor_.health = health_;
        actor_.ai = ai_;
    }
    else
    {
        actors.erase(identity_);
    }
    if (controller_)
    {
        auto& cache_ = playerObservations[identity_];
        if (cache_.controller.lock() != controller_)
        {
            cache_ = {};
        }
        cache_.object = object_;
        cache_.controller = controller_;
        cache_.animation = animation_;
        cache_.health = health_;
        cache_.blackboard = object_->GetComponent<BlackboardComponent>();
    }
    else
    {
        playerObservations.erase(identity_);
    }
}

void CombatObservationSystem::UnregisterObject(GameObject* object_)
{
    BS_PROFILE_SCOPE("CombatObservationSystem::UnregisterObject");
    const auto found_ = objectReferences.find(object_);
    if (found_ != objectReferences.end())
    {
        RemoveNameIndex(object_, found_->second.name);
        objectReferences.erase(found_);
    }
    playerObservations.erase(object_);
    actors.erase(object_);
    playerLayerObjects.erase(std::remove_if(playerLayerObjects.begin(), playerLayerObjects.end(),
        [object_](const auto& weak_)
    {
        const auto value_ = weak_.lock();
        return !value_ || value_.get() == object_;
    }), playerLayerObjects.end());
    ++registrationSerial;
}

void CombatObservationSystem::NotifyObjectNameChanged(GameObject* object_, const std::string& oldName_)
{
    const auto found_ = objectReferences.find(object_);
    if (found_ == objectReferences.end())
    {
        return;
    }
    const auto owner_ = found_->second.object.lock();
    if (!owner_)
    {
        UnregisterObject(object_);
        return;
    }
    RemoveNameIndex(object_, oldName_);
    found_->second.name = owner_->GetName();
    objectsByName[owner_->GetName()].push_back(owner_);
    ++registrationSerial;
}

void CombatObservationSystem::Reset()
{
    BS_PROFILE_SCOPE("CombatObservationSystem::Reset");
    objectReferences.clear();
    objectsByName.clear();
    playerObservations.clear();
    actors.clear();
    playerLayerObjects.clear();
    ++registrationSerial;
    observationClock = 0.0f;
}

std::shared_ptr<GameObject> CombatObservationSystem::FindObject(const std::string& name_) const
{
    const auto found_ = objectsByName.find(name_);
    if (found_ == objectsByName.end())
    {
        return nullptr;
    }
    for (const auto& weak_ : found_->second)
    {
        if (const auto object_ = weak_.lock(); object_ && object_->GetName() == name_ && IsRegisteredObject(object_))
        {
            return object_;
        }
    }
    return nullptr;
}

std::shared_ptr<GameObject> CombatObservationSystem::FindPlayerObject(const std::shared_ptr<GameObject>& exclude_) const
{
    for (const auto& weak_ : playerLayerObjects)
    {
        if (const auto object_ = weak_.lock(); object_ && object_ != exclude_ &&
            object_->GetLayer() == LayerType::Player && IsRegisteredObject(object_))
        {
            return object_;
        }
    }
    return nullptr;
}

bool CombatObservationSystem::IsRegisteredObject(const std::shared_ptr<GameObject>& object_) const
{
    if (!object_)
    {
        return false;
    }
    const auto found_ = objectReferences.find(object_.get());
    return found_ != objectReferences.end() && found_->second.object.lock() == object_;
}

void CombatObservationSystem::BindCombatEvents(AttackSystem& attack_, GuardSystem& guard_, JustGuardSystem& justGuard_)
{
    BS_PROFILE_SCOPE("CombatObservationSystem::BindCombatEvents");
    if (combatEventsBound)
    {
        return;
    }
    const std::weak_ptr<CombatObservationSystem> weak_ = weak_from_this();
    attack_.AddHitCallback([weak_](const AttackHitEvent& event_)
        {
        if (const auto self_ = weak_.lock())
        {
            self_->RecordObservedContact(event_.attacker.lock(), true);
        }
    });
    guard_.AddGuardCallback([weak_](const GuardEvent& event_)
        {
        if (const auto self_ = weak_.lock())
        {
            self_->RecordObservedContact(event_.attacker.lock(), false);
        }
    });
    justGuard_.AddJustGuardCallback([weak_](const JustGuardEvent& event_)
        {
        if (const auto self_ = weak_.lock())
        {
            self_->RecordObservedContact(event_.attacker.lock(), false);
        }
    });
    combatEventsBound = true;
}

void CombatObservationSystem::CaptureFrame(float deltaTime_)
{
    BS_PROFILE_SCOPE("CombatObservationSystem::CaptureFrame");
    observationClock += (std::max)(0.0f, deltaTime_);
    RefreshSnapshots();
}

void CombatObservationSystem::RefreshSnapshots()
{
    BS_PROFILE_SCOPE("CombatObservationSystem::RefreshSnapshots");
    CapturePlayers();
    CaptureActors();
}

void CombatObservationSystem::CaptureActors()
{
    BS_PROFILE_SCOPE("CombatObservationSystem::CaptureActors");
    std::vector<GameObject*> expired_;
    for (auto& [identity_, actor_] : actors)
    {
        const auto object_ = actor_.object.lock();
        if (!object_ || !object_->transform)
        {
            expired_.push_back(identity_);
            continue;
        }
        auto& value_ = actor_.value;
        value_.valid = true;
        value_.position = object_->transform->GetWorldPosition();
        const auto health_ = actor_.health.lock();
        value_.alive = !health_ || !health_->IsDead();
        const auto animation_ = actor_.animation.lock();
        const auto state_ = animation_ ? animation_->GetCurrentState() : nullptr;
        value_.animationState = state_ ? state_->name : std::string();
        const auto ai_ = actor_.ai.lock();
        value_.aiEnabled = ai_ && ai_->IsEnabled() && ai_->IsStarted();
        value_.graphActionName.clear();
        value_.dragonActionId = -1;
        if (const auto human_ = std::dynamic_pointer_cast<HumanBossAIComponent>(ai_))
        {
            value_.graphActionName = human_->GetCurrentGraphActionName();
        }
        if (const auto dragon_ = std::dynamic_pointer_cast<DragonAIComponent>(ai_))
        {
            value_.dragonActionId = static_cast<int>(dragon_->GetCurrentAction());
        }
    }
    // 対象だけを掃除し、Dragonの失効でPlayerやHumanBossの履歴を初期化しない。
    for (auto identity_ : expired_)
    {
        UnregisterObject(identity_);
    }
}

void CombatObservationSystem::RecordObservedContact(const std::shared_ptr<GameObject>& attacker_, bool hit_)
{
    BS_PROFILE_SCOPE("CombatObservationSystem::RecordObservedContact");
    auto actor_ = attacker_;
    while (actor_)
    {
        const auto found_ = actors.find(actor_.get());
        if (found_ != actors.end() && found_->second.object.lock() == actor_)
        {
            ++found_->second.contacts;
            if (hit_)
            {
                ++found_->second.hits;
            }
            return;
        }
        const auto parent_ = actor_->transform ? actor_->transform->parent.lock() : nullptr;
        actor_ = parent_ ? parent_->gameObject.lock() : nullptr;
    }
}

std::uint64_t CombatObservationSystem::GetObservedHitCount(const std::string& name_) const
{
    return GetObservedHitCount(FindObject(name_));
}

std::uint64_t CombatObservationSystem::GetObservedHitCount(const std::shared_ptr<GameObject>& object_) const
{
    const auto found_ = actors.find(object_.get());
    return found_ != actors.end() && IsRegisteredObject(object_) ? found_->second.hits : 0;
}

bool CombatObservationSystem::GetActorActionObservation(const std::string& name_, CombatActorObservation& observation_) const
{
    const auto object_ = FindObject(name_);
    const auto found_ = actors.find(object_.get());
    if (found_ == actors.end() || !found_->second.value.valid)
    {
        return false;
    }
    observation_ = found_->second.value;
    return true;
}

void CombatObservationSystem::Gui()
{
    BS_PROFILE_SCOPE("CombatObservationSystem::Gui");
    ImGui::Text("Registered players: %zu / Combat actors: %zu", playerObservations.size(), actors.size());
    ImGui::Text("Observation time: %.3f", observationClock);
    for (const auto& [identity_, cache_] : playerObservations)
    {
        if (const auto object_ = cache_.object.lock(); object_ && ImGui::TreeNode(object_->GetName().c_str()))
        {
            DrawPlayerActionObservationDebug(object_->GetName());
            ImGui::TreePop();
        }
    }
    for (const auto& [identity_, actor_] : actors)
    {
        if (const auto object_ = actor_.object.lock(); object_ && !actor_.ai.expired())
        {
            ImGui::Text("AI %s: %s / Dragon action %d / Hits %llu", object_->GetName().c_str(), actor_.value.animationState.c_str(),
                actor_.value.dragonActionId, static_cast<unsigned long long>(actor_.hits));
        }
    }
}

// Frame開始時の確定済み状態だけを記録し、入力ボタンの押下を観測対象にしない。
void CombatObservationSystem::CapturePlayers()
{
    BS_PROFILE_SCOPE("CombatObservationSystem::CapturePlayers");
    for (auto& [identity_, cache_] : playerObservations)
    {
        const auto object_ = cache_.object.lock();
        const auto controller_ = cache_.controller.lock();
        const auto animation_ = cache_.animation.lock();
        if (!object_ || !controller_ || !controller_->IsEnabled() || !controller_->IsStarted() || !animation_ || !animation_->IsStarted() || !object_->transform)
        {
            cache_.value.valid = false;
            continue;
        }
        const auto previous_ = cache_.value;
        auto& value_ = cache_.value;
        const auto action_ = controller_->GetCurrentAction();
        const auto group_ = GetPlayerActionGroup(action_);
        const auto state_ = animation_->GetCurrentState();
        value_.valid = true;
        value_.action = static_cast<int>(action_);
        value_.actionName = GetPlayerActionName(action_);
        value_.comboIndex = controller_->GetComboIndex();
        value_.animationState = state_ ? state_->name : std::string();
        value_.animationTime = animation_->GetAnimatorTime();
        value_.position = object_->transform->GetWorldPosition();
        const float yaw_ = object_->transform->GetWorldRotation().y * DirectX::XM_PI / 180.0f;
        value_.forward = { std::sin(yaw_), 0.0f, std::cos(yaw_) };
        const auto health_ = cache_.health.lock();
        value_.alive = !health_ || !health_->IsDead();
        value_.guarding = group_ == PLAYER_ACTION_GROUP::GUARD;
        value_.dodging = group_ == PLAYER_ACTION_GROUP::DODGE;
        value_.weakAttack = action_ == PLAYER_ACTION::WEAK_ATTACK || action_ == PLAYER_ACTION::AIR_WEAK_ATTACK;
        value_.airborne = controller_->IsAirborneForAction();
        value_.heavyAttack = GetPlayerAttackType(action_) == PLAYER_ATTACK_TYPE::STRONG || GetPlayerAttackType(action_) == PLAYER_ATTACK_TYPE::STRONG_CHARGE;
        value_.dodgeDirection = ObservedDodgeDirection::NONE;
        if (value_.dodging)
        {
            DirectX::SimpleMath::Vector2 direction_{};
            if (const auto blackboard_ = cache_.blackboard.lock())
            {
                blackboard_->GetVector2("Anim", "DodgeInput", direction_);
            }
            value_.dodgeDirection = std::abs(direction_.x) > std::abs(direction_.y)
                ? (direction_.x < 0.0f ? ObservedDodgeDirection::LEFT : ObservedDodgeDirection::RIGHT)
                : (direction_.y > 0.0f ? ObservedDodgeDirection::FORWARD : ObservedDodgeDirection::BACK);
        }
        value_.attackPhase = ObservedAttackPhase::NONE;
        value_.timeUntilHit = 0.0f;
        bool hasAttack_ = false;
        bool active_ = false;
        float nextStart_ = 100000.0f;
        float lastEnd_ = 0.0f;
        if (group_ == PLAYER_ACTION_GROUP::ATTACK && state_)
        {
            for (const auto& event_ : state_->events)
            {
                if (event_.eventTemplateType != ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE && event_.startAction.find("EnableHitBox") == std::string::npos)
                {
                    continue;
                }
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
                hasAttack_ = true;
                lastEnd_ = (std::max)(lastEnd_, end_);
                if (value_.animationTime >= start_ && value_.animationTime <= end_)
                {
                    active_ = true;
                }
                if (start_ >= value_.animationTime)
                {
                    nextStart_ = (std::min)(nextStart_, start_);
                }
            }
        }
        if (hasAttack_)
        {
            value_.attackPhase = active_ ? ObservedAttackPhase::ACTIVE : value_.animationTime > lastEnd_ ? ObservedAttackPhase::RECOVERY : ObservedAttackPhase::STARTUP;
            value_.timeUntilHit = nextStart_ < 100000.0f ? nextStart_ - value_.animationTime : 0.0f;
        }
        const bool changed_ = !previous_.valid || previous_.action != value_.action || previous_.comboIndex != value_.comboIndex ||
            previous_.animationState != value_.animationState || (hasAttack_ && value_.animationTime + 0.001f < previous_.animationTime);
        if (changed_)
        {
            ++value_.serial;
            if (hasAttack_)
            {
                cache_.attackStart = observationClock;
                cache_.contactsAtAttackStart = actors.at(identity_).contacts;
            }
            // Idleを挟んだ再入力も、直前の戦闘Actionとの連続使用として数える。
            if (hasAttack_ || value_.guarding || value_.dodging)
            {
                value_.sameActionCount = !value_.history.empty() && value_.history.back().action == value_.action ? previous_.sameActionCount + 1 : 1;
            }
            value_.consecutiveDodges = value_.dodging ? (previous_.history.empty() ? 1 : previous_.consecutiveDodges + 1) : (hasAttack_ || value_.guarding ? 0 : previous_.consecutiveDodges);
            if (hasAttack_ || value_.guarding || value_.dodging)
            {
                value_.history.push_back({ observationClock, value_.action, hasAttack_, value_.weakAttack, value_.heavyAttack, value_.guarding, value_.dodging, value_.dodgeDirection });
                if (value_.history.size() > 10)
                {
                    value_.history.erase(value_.history.begin());
                }
            }
        }
        if (value_.guarding && !previous_.guarding)
        {
            cache_.guardStart = observationClock;
        }
        value_.guardHoldTime = value_.guarding ? observationClock - cache_.guardStart : 0.0f;
        value_.attackAge = hasAttack_ ? observationClock - cache_.attackStart : 0.0f;
        value_.attackMadeContact = actors.at(identity_).contacts > cache_.contactsAtAttackStart;
        value_.history.erase(std::remove_if(value_.history.begin(), value_.history.end(), [this](const ObservedPlayerAction& action_)
        {
            return observationClock - action_.time > observationHistorySeconds;
        }), value_.history.end());
        float attacks_ = 0.0f, weak_ = 0.0f, heavy_ = 0.0f, guards_ = 0.0f, dodges_ = 0.0f;
        for (const auto& actionHistory_ : value_.history)
        {
            attacks_ += actionHistory_.attack;
            weak_ += actionHistory_.weakAttack;
            heavy_ += actionHistory_.heavyAttack;
            guards_ += actionHistory_.guard;
            dodges_ += actionHistory_.dodge;
        }
        const float count_ = (std::max)(1.0f, static_cast<float>(value_.history.size()));
        value_.attackFrequency = attacks_ / count_;
        value_.weakAttackRate = weak_ / (std::max)(1.0f, attacks_);
        value_.heavyAttackRate = heavy_ / (std::max)(1.0f, attacks_);
        value_.guardFrequency = (std::max)(guards_ / count_, value_.guarding ? std::clamp(value_.guardHoldTime / observationHistorySeconds, 0.0f, 1.0f) : 0.0f);
        value_.dodgeFrequency = dodges_ / count_;
    }
}

bool CombatObservationSystem::GetPlayerActionObservation(const std::string& name_, PlayerActionObservation& observation_) const
{
    return GetPlayerActionObservation(FindObject(name_), observation_);
}

bool CombatObservationSystem::GetPlayerActionObservation(const std::shared_ptr<GameObject>& object_, PlayerActionObservation& observation_) const
{
    if (!IsRegisteredObject(object_))
    {
        return false;
    }
    const auto found_ = playerObservations.find(object_.get());
    if (found_ == playerObservations.end() || !found_->second.value.valid)
    {
        return false;
    }
    observation_ = found_->second.overrideEnabled ? found_->second.debugValue : found_->second.value;
    // Debugの知覚上書きでも、位置と生死は実際のゲーム状態を維持する。
    observation_.position = found_->second.value.position;
    observation_.forward = found_->second.value.forward;
    observation_.alive = found_->second.value.alive;
    observation_.airborne = found_->second.value.airborne;
    return true;
}

// 上書きはScene保存へ書かず、調整後に解除できる知覚テストとして扱う。
void CombatObservationSystem::DrawPlayerActionObservationDebug(const std::string& name_)
{
    const auto object_ = FindObject(name_);
    const auto found_ = playerObservations.find(object_.get());
    if (found_ == playerObservations.end())
    {
        ImGui::TextDisabled("Player observation not published yet.");
        return;
    }
    auto& cache_ = found_->second;
    if (ImGui::Checkbox("Override player perception", &cache_.overrideEnabled) && cache_.overrideEnabled)
    {
        cache_.debugValue = cache_.value;
    }
    auto& value_ = cache_.debugValue;
    ImGui::DragFloat("History window seconds", &observationHistorySeconds, 0.1f, 1.0f, 20.0f);
    if (!cache_.overrideEnabled)
    {
        return;
    }
    ImGui::InputInt("Observed Action ID", &value_.action);
    const char* PHASES[] = { "NONE", "STARTUP", "ACTIVE", "RECOVERY" };
    int phase_ = static_cast<int>(value_.attackPhase);
    if (ImGui::Combo("Observed Attack Phase", &phase_, PHASES, 4))
    {
        value_.attackPhase = static_cast<ObservedAttackPhase>(phase_);
    }
    ImGui::Checkbox("Observed Weak Attack", &value_.weakAttack);
    ImGui::Checkbox("Observed Heavy Attack", &value_.heavyAttack);
    ImGui::Checkbox("Observed Guard", &value_.guarding);
    ImGui::Checkbox("Observed Dodge", &value_.dodging);
    ImGui::Checkbox("Observed attack made contact", &value_.attackMadeContact);
    ImGui::SliderFloat("Attack Frequency", &value_.attackFrequency, 0.0f, 1.0f);
    ImGui::SliderFloat("Weak Attack Rate", &value_.weakAttackRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Heavy Attack Rate", &value_.heavyAttackRate, 0.0f, 1.0f);
    ImGui::SliderFloat("Guard Frequency", &value_.guardFrequency, 0.0f, 1.0f);
    ImGui::SliderFloat("Dodge Frequency", &value_.dodgeFrequency, 0.0f, 1.0f);
    ImGui::DragFloat("Attack Age", &value_.attackAge, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Time Until Hit", &value_.timeUntilHit, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Guard Hold Time", &value_.guardHoldTime, 0.05f, 0.0f, 10.0f);
    ImGui::InputInt("Same Action Count", &value_.sameActionCount);
    ImGui::InputInt("Consecutive Dodges", &value_.consecutiveDodges);
    const char* DIRECTIONS[] = { "NONE", "LEFT", "RIGHT", "BACK", "FORWARD" };
    int direction_ = static_cast<int>(value_.dodgeDirection);
    if (ImGui::Combo("Observed Dodge Direction", &direction_, DIRECTIONS, 5))
    {
        value_.dodgeDirection = static_cast<ObservedDodgeDirection>(direction_);
    }
    if (ImGui::Button("Publish next simulated action"))
    {
        ++value_.serial;
    }
}
