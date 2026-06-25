#include "AttackSystem.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/HurtBox.h"

#include <cmath>

void AttackSystem::BeginFrame()
{
    frameHitEvents.clear();
}

void AttackSystem::Clear()
{
    frameHitEvents.clear();
}

void AttackSystem::HandleHit(HitBox& hitBox_, HurtBox& hurtBox_)
{
    AttackHitContact contact_{};
    HandleHit(hitBox_, hurtBox_, contact_);
}

void AttackSystem::HandleHit(
    HitBox& hitBox_,
    HurtBox& hurtBox_,
    const AttackHitContact& contact_)
{
    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetGameObject();
    const std::shared_ptr<GameObject> target_ = hurtBox_.GetGameObject();
    if (!attacker_ || !target_ || IsSameHierarchy(attacker_, target_)) return;
    if (!hitBox_.IsEnabled() || !hurtBox_.IsEnabled()) return;
    if (!hurtBox_.CanReceiveHit(hitBox_)) return;

    hurtBox_.RecordHit(hitBox_);

    AttackHitEvent event_;
    event_.attacker = attacker_;
    event_.target = target_;
    event_.hitBox = &hitBox_;
    event_.hurtBox = &hurtBox_;
    event_.damage = hitBox_.damage * hurtBox_.damageMultiplier;
    event_.hitStopTime = hitBox_.hitStopTime;
    event_.hitStopDir = hitBox_.hitStopDir;
    event_.hitStopLayerMask = hitBox_.hitStopLayerMask;
    event_.knockBackPower = hitBox_.knockBackPower;
    event_.hitPosition = contact_.position;
    event_.hitNormal = contact_.normal;
    event_.hasHitContact = contact_.isValid;
    event_.attackTag = hitBox_.attackTag;
    event_.hurtTag = hurtBox_.hurtTag;
    event_.hitParticleAssetPath = hitBox_.hitParticleAssetPath;
    event_.hitParticleEmitterName = hitBox_.hitParticleEmitterName;
    event_.hitParticleLifeTime = hitBox_.hitParticleLifeTime;
    event_.hitParticleNormalOffset = hitBox_.hitParticleNormalOffset;
    event_.hitMeshVFXAssetPath = hitBox_.hitMeshVFXAssetPath;
    event_.hitMeshVFXLifeTime = hitBox_.hitMeshVFXLifeTime;
    event_.hitMeshVFXNormalOffset = hitBox_.hitMeshVFXNormalOffset;
    event_.hitSoundPath = hitBox_.hitSoundPath;
    event_.hitSoundVolume = hitBox_.hitSoundVolume;
    event_.hitSoundMuffled = hitBox_.hitSoundMuffled;
    event_.hitSoundMuffledCutoff = hitBox_.hitSoundMuffledCutoff;

    frameHitEvents.push_back(event_);
    PublishHitToBlackboard(event_);

    for (const HitCallback& callback_ : hitCallbacks)
    {
        if (callback_) callback_(event_);
    }
}

void AttackSystem::AddHitCallback(HitCallback callback_)
{
    if (callback_) hitCallbacks.push_back(std::move(callback_));
}

bool AttackSystem::IsSameHierarchy(
    const std::shared_ptr<GameObject>& objectA_,
    const std::shared_ptr<GameObject>& objectB_) const
{
    return GetHierarchyRoot(objectA_) == GetHierarchyRoot(objectB_);
}

std::shared_ptr<GameObject> AttackSystem::GetHierarchyRoot(
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

void AttackSystem::PublishHitToBlackboard(const AttackHitEvent& event_)
{
    const std::shared_ptr<GameObject> target_ = event_.target.lock();
    if (!target_) return;

    std::shared_ptr<GameObject> receiver_ = target_;
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

    blackboard_->SetFloat("Hit", "Damage", event_.damage);
    blackboard_->SetFloat("Hit", "HitStopTime", event_.hitStopTime);
    blackboard_->SetFloat("Hit", "KnockBackPower", event_.knockBackPower);
    blackboard_->SetVector3("Hit", "Position", event_.hitPosition);
    blackboard_->SetVector3("Hit", "Normal", event_.hitNormal);
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    const std::shared_ptr<GameObject> attackerRoot_ = GetHierarchyRoot(attacker_);
    const std::shared_ptr<GameObject> targetRoot_ = GetHierarchyRoot(target_);
    if (attackerRoot_ && targetRoot_ &&
        attackerRoot_->transform && targetRoot_->transform)
    {
        const DirectX::XMFLOAT3 attackerPosition_ =
            attackerRoot_->transform->GetWorldPosition();
        const DirectX::XMFLOAT3 targetPosition_ =
            targetRoot_->transform->GetWorldPosition();
        const float directionX_ = targetPosition_.x - attackerPosition_.x;
        const float directionZ_ = targetPosition_.z - attackerPosition_.z;
        const float length_ = std::sqrt(
            directionX_ * directionX_ + directionZ_ * directionZ_);
        if (length_ > 0.0001f)
        {
            blackboard_->SetVector2(
                "Hit",
                "Direction",
                DirectX::SimpleMath::Vector2(
                    directionX_ / length_,
                    directionZ_ / length_));
        }
    }
    blackboard_->SetTrigger("Hit", "Received");

    if (receiver_->localRegistry.HasAction("OnHit"))
    {
        receiver_->localRegistry.Call("OnHit");
    }
}

