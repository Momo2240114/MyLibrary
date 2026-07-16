#include "PrecompiledHeader.h"

#include "GuardSystem.h"

#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/GuardCollider.h"

#include <string>
#include <utility>

namespace
{
    constexpr const char* DEFENDER_NORMAL_GUARD_ACTION = "OnGuardSucceeded";
    constexpr const char* DEFENDER_JUST_GUARD_ACTION = "OnJustGuardSucceeded";
    constexpr const char* ATTACKER_NORMAL_GUARDED_ACTION = "OnAttackGuarded";
    constexpr const char* ATTACKER_JUST_GUARDED_ACTION = "OnAttackJustGuarded";
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
    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetAttackOwner();
    const std::shared_ptr<GameObject> defender_ = guardCollider_.GetGuardOwner();

    if (!attacker_ || !defender_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[Guard] Rejected: attacker or defender missing.");
        return false;
    }
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

    GuardEvent event_{};
    event_.defender = defender_;
    event_.attacker = attacker_;
    event_.guardCollider = &guardCollider_;
    event_.hitBox = &hitBox_;
    event_.resultType = guardCollider_.IsJustGuardEnabled()
        ? GUARD_RESULT_TYPE::JUST_GUARD
        : GUARD_RESULT_TYPE::NORMAL_GUARD;
    event_.attackSourceType = hitBox_.attackSourceType;
    event_.guardPosition = contact_.position;
    event_.attackNormal = contact_.normal;
    event_.hasContact = contact_.isValid;
    event_.attackInstanceId = hitBox_.attackInstanceId;
    event_.hitGroupId = hitBox_.hitGroupId;
    event_.teamId = hitBox_.teamId;
    event_.damage = hitBox_.damage;
    event_.impactPower = hitBox_.impactPower;
    event_.knockBackPower = hitBox_.knockBackPower;
    event_.sourceAction = hitBox_.sourceAction;
    event_.attackTag = hitBox_.attackTag;

    hitBox_.Deactivate();

    frameGuardEvents.push_back(event_);
    PublishDefenderGuardResult(event_);
    PublishAttackerGuardResult(event_);

    for (const GuardCallback& callback_ : guardCallbacks)
    {
        if (callback_) callback_(event_);
    }

    BS_LOG_SUCCESS(
        LOG_CATEGORY::GAME,
        event_.resultType == GUARD_RESULT_TYPE::JUST_GUARD
            ? "[Guard] JustGuard accepted."
            : "[Guard] Normal guard accepted.");

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

void GuardSystem::PublishDefenderGuardResult(const GuardEvent& event_)
{
    if (event_.resultType == GUARD_RESULT_TYPE::JUST_GUARD)
    {
        PublishJustGuardToDefender(event_);
        return;
    }

    PublishNormalGuardToDefender(event_);
}

void GuardSystem::PublishAttackerGuardResult(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    if (!attacker_ || !event_.hitBox || !event_.hitBox->ShouldNotifyAttackerOnGuard())
    {
        return;
    }

    if (event_.resultType == GUARD_RESULT_TYPE::JUST_GUARD)
    {
        PublishJustGuardToAttacker(event_);
        return;
    }

    PublishNormalGuardToAttacker(event_);
}

void GuardSystem::PublishNormalGuardToDefender(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(defender_);
    if (!receiver_) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetBool("Guard", "GuardSucceeded", true);
    blackboard_->SetBool("Guard", "JustGuardSucceeded", false);
    blackboard_->SetTrigger("Guard", "GuardTrigger");
    blackboard_->SetInt(
        "Guard",
        "GuardResultType",
        static_cast<int>(GUARD_RESULT_TYPE::NORMAL_GUARD));
    blackboard_->SetInt("Guard", "AttackInstanceId", static_cast<int>(event_.attackInstanceId));
    blackboard_->SetInt("Guard", "HitGroupId", static_cast<int>(event_.hitGroupId));
    blackboard_->SetString("Guard", "GuardedAttackTag", event_.attackTag);
    blackboard_->SetString("Guard", "GuardedSourceAction", event_.sourceAction);
    blackboard_->SetVector3("Guard", "Position", event_.guardPosition);
    blackboard_->SetVector3("Guard", "Normal", event_.attackNormal);
    blackboard_->SetFloat("Guard", "ReceivedDamage", event_.damage);
    blackboard_->SetFloat("Guard", "ReceivedImpactPower", event_.impactPower);
    blackboard_->SetFloat("Guard", "ReceivedKnockBackPower", event_.knockBackPower);

    receiver_->ExecuteAction(GetDefenderActionName(event_.resultType));
}

void GuardSystem::PublishJustGuardToDefender(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(defender_);
    if (!receiver_) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetBool("Guard", "GuardSucceeded", true);
    blackboard_->SetBool("Guard", "JustGuardSucceeded", true);
    blackboard_->SetTrigger("Guard", "GuardTrigger");
    blackboard_->SetTrigger("Guard", "JustGuardTrigger");
    blackboard_->SetBool("Guard", "CanCounter", true);
    blackboard_->SetInt(
        "Guard",
        "GuardResultType",
        static_cast<int>(GUARD_RESULT_TYPE::JUST_GUARD));
    blackboard_->SetInt("Guard", "AttackInstanceId", static_cast<int>(event_.attackInstanceId));
    blackboard_->SetInt("Guard", "HitGroupId", static_cast<int>(event_.hitGroupId));
    blackboard_->SetString("Guard", "GuardedAttackTag", event_.attackTag);
    blackboard_->SetString("Guard", "GuardedSourceAction", event_.sourceAction);
    blackboard_->SetVector3("Guard", "Position", event_.guardPosition);
    blackboard_->SetVector3("Guard", "Normal", event_.attackNormal);
    blackboard_->SetFloat("Guard", "ReceivedDamage", event_.damage);
    blackboard_->SetFloat("Guard", "ReceivedImpactPower", event_.impactPower);
    blackboard_->SetFloat("Guard", "ReceivedKnockBackPower", event_.knockBackPower);

    receiver_->ExecuteAction(GetDefenderActionName(event_.resultType));
}

void GuardSystem::PublishNormalGuardToAttacker(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(attacker_);
    if (!receiver_) return;

    if (const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>())
    {
        blackboard_->SetBool("GuardReaction", "AttackGuarded", true);
        blackboard_->SetBool("GuardReaction", "AttackJustGuarded", false);
        blackboard_->SetTrigger("GuardReaction", "AttackGuardedTrigger");
    }

    receiver_->ExecuteAction(GetAttackerActionName(event_.resultType));
}

void GuardSystem::PublishJustGuardToAttacker(const GuardEvent& event_)
{
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(attacker_);
    if (!receiver_) return;

    if (const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>())
    {
        blackboard_->SetBool("GuardReaction", "AttackGuarded", true);
        blackboard_->SetBool("GuardReaction", "AttackJustGuarded", true);
        blackboard_->SetTrigger("GuardReaction", "AttackJustGuardedTrigger");
        blackboard_->SetBool("GuardReaction", "ShouldCancelAttack", true);
    }

    receiver_->ExecuteAction(GetAttackerActionName(event_.resultType));
}

std::string GuardSystem::GetDefenderActionName(GUARD_RESULT_TYPE resultType_) const
{
    return resultType_ == GUARD_RESULT_TYPE::JUST_GUARD
        ? DEFENDER_JUST_GUARD_ACTION
        : DEFENDER_NORMAL_GUARD_ACTION;
}

std::string GuardSystem::GetAttackerActionName(GUARD_RESULT_TYPE resultType_) const
{
    return resultType_ == GUARD_RESULT_TYPE::JUST_GUARD
        ? ATTACKER_JUST_GUARDED_ACTION
        : ATTACKER_NORMAL_GUARDED_ACTION;
}

std::shared_ptr<GameObject> GuardSystem::FindBlackboardReceiver(
    std::shared_ptr<GameObject> object_) const
{
    if (!object_) return nullptr;

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
