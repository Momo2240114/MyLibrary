#include "JustDodgeSystem.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/JustDodgeCollider.h"

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

    if (!attacker_ || !dodger_) return;
    if (!hitBox_.IsEnabled() || !justDodgeCollider_.IsEnabled()) return;

    // îOÇÃÇΩÇﬂSystemë§Ç≈Ç‡ìØäKëwÇèúäO
    if (IsSameHierarchy(attacker_, dodger_)) return;

    if (!justDodgeCollider_.CanReactToAttack(hitBox_)) return;

    if (!RegisterJustDodge(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        dodger_.get(),
        false))
    {
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
    PublishJustDodgeToBlackboard(event_);

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

    if (receiver_->localRegistry.HasAction("OnJustDodge"))
    {
        receiver_->localRegistry.Call("OnJustDodge");
    }
}