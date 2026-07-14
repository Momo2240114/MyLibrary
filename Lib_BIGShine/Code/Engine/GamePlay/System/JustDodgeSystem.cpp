#include "JustDodgeSystem.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/JustDodgeCollider.h"

#include <string>

void JustDodgeSystem::BeginFrame()
{
    frameJustDodgeEvents.clear();
}

void JustDodgeSystem::Clear()
{
    frameJustDodgeEvents.clear();
    registeredDodgeTargets.clear();
}

void JustDodgeSystem::ClearAttackInstance(uint32_t attackInstanceId_)
{
    if (attackInstanceId_ == 0u) return;

    for (auto iterator_ = registeredDodgeTargets.begin();
        iterator_ != registeredDodgeTargets.end();)
    {
        if (static_cast<uint32_t>(iterator_->first >> 32u) == attackInstanceId_)
        {
            iterator_ = registeredDodgeTargets.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }
}

void JustDodgeSystem::ClearAttackHitGroup(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_)
{
    if (attackInstanceId_ == 0u) return;

    const uint64_t dodgeKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);

    registeredDodgeTargets.erase(dodgeKey_);
}

void JustDodgeSystem::HandleJustDodge(
    HitBox& hitBox_,
    JustDodgeCollider& justDodgeCollider_)
{
    JustDodgeContact contact_{};
    HandleJustDodge(hitBox_, justDodgeCollider_, contact_);
}

void JustDodgeSystem::HandleJustDodge(
    HitBox& hitBox_,
    JustDodgeCollider& justDodgeCollider_,
    const JustDodgeContact& contact_)
{
    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetAttackOwner();
    const std::shared_ptr<GameObject> dodger_ = justDodgeCollider_.GetGameObject();

    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "[JustDodge] HandleJustDodge entered. attackInstance="
        + std::to_string(hitBox_.attackInstanceId)
        + " hitGroup="
        + std::to_string(hitBox_.hitGroupId));

    if (!attacker_ || !dodger_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Rejected: attacker or dodger missing.");
        return;
    }
    if (!hitBox_.IsEnabled() || !justDodgeCollider_.IsEnabled())
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Rejected: HitBox or JustDodgeCollider disabled.");
        return;
    }

    // 念のためSystem側でも同階層を除外
    if (IsSameHierarchy(attacker_, dodger_))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Rejected: same hierarchy.");
        return;
    }

    if (!justDodgeCollider_.CanReactToAttack(hitBox_))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Rejected: collider cannot react to this attack.");
        return;
    }

    if (!RegisterJustDodge(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        dodger_.get(),
        false))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Rejected: already registered for this attack.");
        return;
    }

    justDodgeCollider_.RecordDodgedAttack(hitBox_);

    JustDodgeEvent event_{};
    event_.dodger = dodger_;
    event_.attacker = attacker_;
    event_.justDodgeCollider = &justDodgeCollider_;
    event_.hitBox = &hitBox_;
    event_.dodgePosition = contact_.position;
    event_.attackNormal = contact_.normal;
    event_.hasContact = contact_.isValid;
    event_.attackInstanceId = hitBox_.attackInstanceId;
    event_.hitGroupId = hitBox_.hitGroupId;
    event_.teamId = hitBox_.teamId;
    event_.sourceAction = hitBox_.sourceAction;
    event_.attackTag = hitBox_.attackTag;

    frameJustDodgeEvents.push_back(event_);
    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[JustDodge] Event accepted. Publishing blackboard/action.");
    PublishJustDodgeToBlackboard(event_);

    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "[JustDodge] Invoking registered callbacks. count="
        + std::to_string(justDodgeCallbacks.size()));

    for (const JustDodgeCallback& callback_ : justDodgeCallbacks)
    {
        if (callback_) callback_(event_);
    }
}

bool JustDodgeSystem::RegisterJustDodge(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_,
    const GameObject* dodger_,
    bool allowMultiDodge_)
{
    if (allowMultiDodge_ || attackInstanceId_ == 0u || dodger_ == nullptr)
    {
        return true;
    }

    const uint64_t dodgeKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);

    return registeredDodgeTargets[dodgeKey_].insert(dodger_).second;
}

void JustDodgeSystem::AddJustDodgeCallback(JustDodgeCallback callback_)
{
    if (callback_) justDodgeCallbacks.push_back(std::move(callback_));
}

bool JustDodgeSystem::IsSameHierarchy(
    const std::shared_ptr<GameObject>& objectA_,
    const std::shared_ptr<GameObject>& objectB_) const
{
    return GetHierarchyRoot(objectA_) == GetHierarchyRoot(objectB_);
}

std::shared_ptr<GameObject> JustDodgeSystem::GetHierarchyRoot(
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

void JustDodgeSystem::PublishJustDodgeToBlackboard(
    const JustDodgeEvent& event_)
{
    const std::shared_ptr<GameObject> dodger_ = event_.dodger.lock();
    if (!dodger_) return;

    std::shared_ptr<GameObject> receiver_ = dodger_;
    std::shared_ptr<BlackboardComponent> blackboard_;

    while (receiver_)
    {
        blackboard_ = receiver_->GetComponent<BlackboardComponent>();
        if (blackboard_ || !receiver_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            receiver_->transform->parent.lock();

        if (!parentTransform_) break;

        receiver_ = parentTransform_->gameObject.lock();
    }

    if (!receiver_) return;

    if (!blackboard_)
    {
        blackboard_ = receiver_->GetOrAddComponent<BlackboardComponent>();
    }

    if (!blackboard_) return;

    blackboard_->SetBool("Dodge", "JustDodgeSucceeded", true);
    blackboard_->SetTrigger("Dodge", "JustDodgeTrigger");
    blackboard_->SetBool("Dodge", "CanCounter", true);
    blackboard_->SetString("Dodge", "DodgedAttackTag", event_.attackTag);
    blackboard_->SetString("Dodge", "DodgedSourceAction", event_.sourceAction);
    blackboard_->SetInt("Dodge", "AttackInstanceId", static_cast<int>(event_.attackInstanceId));
    blackboard_->SetInt("Dodge", "HitGroupId", static_cast<int>(event_.hitGroupId));
    blackboard_->SetVector3("Dodge", "Position", event_.dodgePosition);
    blackboard_->SetVector3("Dodge", "Normal", event_.attackNormal);

    std::shared_ptr<GameObject> actionReceiver_ = dodger_;
    bool onJustDodgeCalled_ = false;
    while (actionReceiver_)
    {
        const std::string actionName_ = actionReceiver_->BuildActionName("OnJustDodge");
        if (actionReceiver_->localRegistry.HasAction(actionName_) ||
            actionReceiver_->localRegistry.HasAction("OnJustDodge"))
        {
            BS_LOG_SUCCESS(
                LOG_CATEGORY::GAME,
                "[JustDodge] Calling OnJustDodge on "
                + actionReceiver_->GetName());
            actionReceiver_->ExecuteAction("OnJustDodge");
            onJustDodgeCalled_ = true;
            break;
        }

        if (!actionReceiver_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            actionReceiver_->transform->parent.lock();

        if (!parentTransform_) break;

        actionReceiver_ = parentTransform_->gameObject.lock();
    }

    if (!onJustDodgeCalled_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] OnJustDodge action receiver not found.");
    }
}
