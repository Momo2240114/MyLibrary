#include "PrecompiledHeader.h"

#include "GuardSystem.h"

#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"

#include <string>
#include <utility>

namespace
{
    constexpr const char* DEFENDER_NORMAL_GUARD_ACTION = "OnGuardSucceeded";
    constexpr const char* ATTACKER_NORMAL_GUARDED_ACTION = "OnAttackGuarded";
}

void GuardSystem::BeginFrame()
{
    frameGuardEvents.clear();
}

void GuardSystem::Clear()
{
    frameGuardEvents.clear();
    registeredGuardTargets.clear();
}

void GuardSystem::ClearAttackInstance(uint32_t attackInstanceId_)
{
    if (attackInstanceId_ == 0u) return;

    // 攻撃IDが終わったら、同じ攻撃に対するガード履歴も破棄する。
    for (auto iterator_ = registeredGuardTargets.begin();
        iterator_ != registeredGuardTargets.end();)
    {
        if (static_cast<uint32_t>(iterator_->first >> 32u) == attackInstanceId_)
        {
            iterator_ = registeredGuardTargets.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }
}

void GuardSystem::ClearAttackHitGroup(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_)
{
    if (attackInstanceId_ == 0u) return;

    const uint64_t guardKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);

    registeredGuardTargets.erase(guardKey_);
}

bool GuardSystem::HandleGuard(HitBox& hitBox_, GuardCollider& guardCollider_)
{
    GuardContact contact_{};
    return HandleGuard(hitBox_, guardCollider_, contact_);
}

bool GuardSystem::HandleGuard(
    HitBox& hitBox_,
    GuardCollider& guardCollider_,
    const GuardContact& contact_)
{
    BS_PROFILE_SCOPE("GuardSystem::HandleGuard");
    // 攻撃側と防御側の有効性、同一階層、通常ガード可能条件を確認してからGuardEvent化する。
    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetAttackOwner();
    const std::shared_ptr<GameObject> defender_ = guardCollider_.GetGuardOwner();

    if (!attacker_ || !defender_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[Guard] Rejected: attacker or defender missing.");
        return false;
    }
    // 扇形の安全側では広域Sphereが重なってもガード成立にしない。
    if (!hitBox_.IsWithinAttackSector(defender_.get())) return false;
    if (!hitBox_.IsEnabled() || !guardCollider_.IsEnabled())
    {
        return false;
    }
    if (IsSameHierarchy(attacker_, defender_))
    {
        return false;
    }
    if (!guardCollider_.CanGuardAttack(hitBox_))
    {
        return false;
    }
    if (!RegisterGuard(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        defender_.get(),
        false))
    {
        return false;
    }

    guardCollider_.RecordGuardedAttack(hitBox_);
    // Playerが受付を閉じた後も、受け止め済みの同一Activationを通常Hitとして再適用しない。
    if (guardCollider_.GuardsBodyContact()) hitBox_.RegisterHit(defender_.get());

    // 成立したガード情報を、受け手/攻撃側/CameraActionへ配るためのイベントにまとめる。
    GuardEvent event_{};
    event_.defender = defender_;
    event_.attacker = attacker_;
    event_.guardCollider = &guardCollider_;
    event_.hitBox = &hitBox_;
    event_.attackSourceType = hitBox_.attackSourceType;
    event_.guardPosition = contact_.position;
    event_.attackNormal = contact_.normal;
    event_.hasContact = contact_.isValid;
    event_.guardBroken = hitBox_.guardBreak;
    event_.attackInstanceId = hitBox_.attackInstanceId;
    event_.hitGroupId = hitBox_.hitGroupId;
    event_.teamId = hitBox_.teamId;
    event_.damage = hitBox_.damage;
    if (const auto hurtBox_ = defender_->GetComponent<HurtBox>())
        event_.damage = AttackSystem::CalculateGuardDamage(hitBox_, *hurtBox_);
    event_.impactPower = hitBox_.impactPower;
    event_.knockBackPower = hitBox_.knockBackPower;
    event_.sourceAction = hitBox_.sourceAction;
    event_.attackTag = hitBox_.attackTag;

    frameGuardEvents.push_back(event_);
    // GuardBreakのHP処理を一度だけ行い、通常HitReactionとの二重再生を防ぐ。
    if (event_.guardBroken)
    {
        if (const auto health_ = defender_->GetComponent<HealthComponent>())
            health_->ApplyDamage(event_.damage * guardCollider_.GetGuardBreakDamageRate());
        if (defender_->manager && hitBox_.hitStopDuration > 0.0f)
            if (const auto time_ = defender_->GetManager().GetTimeManager())
                time_->TriggerHitStop(hitBox_.hitStopLayerMask == 0u ? static_cast<uint32_t>(TarGetLayerType::All) : hitBox_.hitStopLayerMask,
                    hitBox_.hitStopDuration, hitBox_.hitStopScale);
    }
    PublishGuardToDefender(event_);
    if (!event_.guardBroken) PublishGuardToAttacker(event_);

    for (const GuardCallback& callback_ : guardCallbacks)
    {
        if (callback_) callback_(event_);
    }

    BS_LOG_SUCCESS(
        LOG_CATEGORY::GAME,
        event_.guardBroken ? "[Guard] Guard break accepted." : "[Guard] Normal guard accepted.");

    return true;
}

void GuardSystem::AddGuardCallback(GuardCallback callback_)
{
    if (callback_) guardCallbacks.push_back(std::move(callback_));
}

bool GuardSystem::RegisterGuard(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_,
    const GameObject* defender_,
    bool allowMultiGuard_)
{
    if (allowMultiGuard_ || attackInstanceId_ == 0u || defender_ == nullptr)
    {
        return true;
    }

    const uint64_t guardKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);

    // 同じ攻撃判定で同じ防御者へ何度もガード成立しないよう履歴を持つ。
    return registeredGuardTargets[guardKey_].insert(defender_).second;
}

bool GuardSystem::IsSameHierarchy(
    const std::shared_ptr<GameObject>& objectA_,
    const std::shared_ptr<GameObject>& objectB_) const
{
    return GetHierarchyRoot(objectA_) == GetHierarchyRoot(objectB_);
}

std::shared_ptr<GameObject> GuardSystem::GetHierarchyRoot(
    std::shared_ptr<GameObject> object_) const
{
    while (object_ && object_->transform)
    {
        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;

        object_ = parentObject_;
    }

    return object_;
}

void GuardSystem::PublishGuardToAttacker(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    if (!attacker_ || !event_.hitBox || !event_.hitBox->ShouldNotifyAttackerOnGuard())
    {
        return;
    }

    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(attacker_);
    if (!receiver_) return;

    if (const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>())
    {
        blackboard_->SetBool("GuardReaction", "AttackGuarded", true);
        blackboard_->SetBool("GuardReaction", "AttackJustGuarded", false);
        blackboard_->SetTrigger("GuardReaction", "AttackGuardedTrigger");
    }

    receiver_->ExecuteAction(ATTACKER_NORMAL_GUARDED_ACTION);
}

void GuardSystem::PublishGuardToDefender(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(defender_);
    if (!receiver_) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetBool("Guard", "GuardSucceeded", !event_.guardBroken);
    blackboard_->SetBool("Guard", "GuardBroken", event_.guardBroken);
    blackboard_->SetBool("Guard", "JustGuardSucceeded", false);
    blackboard_->SetTrigger("Guard", "GuardTrigger");
    blackboard_->SetInt("Guard", "AttackInstanceId", static_cast<int>(event_.attackInstanceId));
    blackboard_->SetInt("Guard", "HitGroupId", static_cast<int>(event_.hitGroupId));
    blackboard_->SetString("Guard", "GuardedAttackTag", event_.attackTag);
    blackboard_->SetString("Guard", "GuardedSourceAction", event_.sourceAction);
    blackboard_->SetVector3("Guard", "Position", event_.guardPosition);
    blackboard_->SetVector3("Guard", "Normal", event_.attackNormal);
    blackboard_->SetBool("Guard", "HasContact", event_.hasContact);
    blackboard_->SetFloat("Guard", "ReceivedDamage", event_.damage);
    blackboard_->SetFloat("Guard", "ReceivedImpactPower", event_.impactPower);
    blackboard_->SetFloat("Guard", "ReceivedKnockBackPower", event_.knockBackPower);

    receiver_->ExecuteAction(event_.guardBroken ? "OnGuardBreak" : DEFENDER_NORMAL_GUARD_ACTION);
}

std::shared_ptr<GameObject> GuardSystem::FindBlackboardReceiver(
    std::shared_ptr<GameObject> object_) const
{
    if (!object_) return nullptr;

    // Colliderが子Objectに付いていても、親側のBlackboardを優先してイベントを届ける。
    std::shared_ptr<GameObject> fallback_ = object_;
    while (object_)
    {
        if (object_->GetComponent<BlackboardComponent>())
        {
            return object_;
        }

        if (!object_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;

        fallback_ = parentObject_;
        object_ = parentObject_;
    }

    return fallback_;
}
