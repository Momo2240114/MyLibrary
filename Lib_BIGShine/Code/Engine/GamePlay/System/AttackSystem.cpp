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
    registeredHitTargets.clear();
}

uint32_t AttackSystem::CreateAttackInstance()
{
    const uint32_t attackInstanceId_ = nextAttackInstanceId++;
    if (nextAttackInstanceId == 0u) nextAttackInstanceId = 1u;
    return attackInstanceId_;
}

void AttackSystem::ClearAttackInstance(uint32_t attackInstanceId_)
{
    if (attackInstanceId_ == 0u) return;
    for (auto iterator_ = registeredHitTargets.begin(); iterator_ != registeredHitTargets.end();)
    {
        if (static_cast<uint32_t>(iterator_->first >> 32u) == attackInstanceId_)
        {
            iterator_ = registeredHitTargets.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }
}

void AttackSystem::ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_)
{
    if (attackInstanceId_ == 0u) return;
    const uint64_t hitKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);
    registeredHitTargets.erase(hitKey_);
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
    const std::shared_ptr<GameObject> hitBoxObject_ = hitBox_.GetGameObject();
    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetAttackOwner();
    const std::shared_ptr<GameObject> target_ = hurtBox_.GetGameObject();
    if (!hitBoxObject_ || !attacker_ || !target_) return;
    if (!hitBox_.canHitOwner && IsSameHierarchy(attacker_, target_)) return;
    if (!hitBox_.IsEnabled() || !hurtBox_.IsEnabled()) return;
    if (!hitBox_.CanHit(target_.get())) return;
    if (!hurtBox_.CanReceiveHit(hitBox_)) return;
    if (!RegisterAttackHit(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        target_.get(),
        hitBox_.allowMultiHit)) return;

    hitBox_.RegisterHit(target_.get());
    hurtBox_.RecordHit(hitBox_);

    AttackHitEvent event_;
    event_.attacker = attacker_;
    event_.target = target_;
    event_.hitBox = &hitBox_;
    event_.hurtBox = &hurtBox_;
    event_.damage = hitBox_.damage * hurtBox_.damageMultiplier;
    event_.hitStopDuration = hitBox_.hitStopDuration;
    event_.hitStopScale = hitBox_.hitStopScale;
    event_.hitStopLayerMask = hitBox_.hitStopLayerMask;
    event_.knockBackPower = hitBox_.knockBackPower;
    event_.hitPosition = contact_.position;
    event_.hitNormal = contact_.normal;
    event_.hasHitContact = contact_.isValid;
    event_.attackInstanceId = hitBox_.attackInstanceId;
    event_.hitGroupId = hitBox_.hitGroupId;
    event_.sourceAction = hitBox_.sourceAction;
    event_.teamId = hitBox_.teamId;
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

bool AttackSystem::RegisterAttackHit(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_,
    const GameObject* target_,
    bool allowMultiHit_)
{
    if (allowMultiHit_ || attackInstanceId_ == 0u || target_ == nullptr) return true;
    const uint64_t hitKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);
    return registeredHitTargets[hitKey_].insert(target_).second;
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
    blackboard_->SetFloat("Hit", "HitStopDuration", event_.hitStopDuration);
    blackboard_->SetFloat("Hit", "HitStopScale", event_.hitStopScale);
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

