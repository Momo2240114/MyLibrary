#include "CollisionStepSystem.h"

#include "ColliderComponent.h"
#include "HitBox.h"
#include "HurtBox.h"
#include "PhysXSystem.h"
#include "Engine/GamePlay/System/JustDodgeSystem.h"
#include "Engine/Physics/JustDodgeCollider.h"


#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/Math/CollisionMath.h"


#include <algorithm>
#include <cmath>

namespace
{
    float Clamp01(float value_)
    {
        return (std::max)(0.0f, (std::min)(value_, 1.0f));
    }

    void ClosestPointsOnSegments(
        DirectX::XMVECTOR startA_,
        DirectX::XMVECTOR endA_,
        DirectX::XMVECTOR startB_,
        DirectX::XMVECTOR endB_,
        DirectX::XMVECTOR& closestA_,
        DirectX::XMVECTOR& closestB_)
    {
        using namespace DirectX;

        const XMVECTOR segmentA_ = XMVectorSubtract(endA_, startA_);
        const XMVECTOR segmentB_ = XMVectorSubtract(endB_, startB_);
        const XMVECTOR startDelta_ = XMVectorSubtract(startA_, startB_);

        const float a_ = XMVectorGetX(XMVector3Dot(segmentA_, segmentA_));
        const float b_ = XMVectorGetX(XMVector3Dot(segmentA_, segmentB_));
        const float c_ = XMVectorGetX(XMVector3Dot(segmentB_, segmentB_));
        const float d_ = XMVectorGetX(XMVector3Dot(segmentA_, startDelta_));
        const float e_ = XMVectorGetX(XMVector3Dot(segmentB_, startDelta_));
        const float denominator_ = a_ * c_ - b_ * b_;

        float parameterA_ = 0.0f;
        float parameterB_ = 0.0f;

        if (a_ <= 0.000001f && c_ <= 0.000001f)
        {
            closestA_ = startA_;
            closestB_ = startB_;
            return;
        }

        if (a_ <= 0.000001f)
        {
            parameterB_ = Clamp01(e_ / c_);
        }
        else if (c_ <= 0.000001f)
        {
            parameterA_ = Clamp01(-d_ / a_);
        }
        else
        {
            if (denominator_ != 0.0f)
            {
                parameterA_ = Clamp01((b_ * e_ - c_ * d_) / denominator_);
            }

            parameterB_ = (b_ * parameterA_ + e_) / c_;
            if (parameterB_ < 0.0f)
            {
                parameterB_ = 0.0f;
                parameterA_ = Clamp01(-d_ / a_);
            }
            else if (parameterB_ > 1.0f)
            {
                parameterB_ = 1.0f;
                parameterA_ = Clamp01((b_ - d_) / a_);
            }
        }

        closestA_ = XMVectorAdd(startA_, XMVectorScale(segmentA_, parameterA_));
        closestB_ = XMVectorAdd(startB_, XMVectorScale(segmentB_, parameterB_));
    }

    AttackHitContact MakeAttackHitContact(
        const Capsule& hitCapsule_,
        const Capsule& hurtCapsule_,
        const CollisionManifold& manifold_)
    {
        using namespace DirectX;

        XMVECTOR closestHit_{};
        XMVECTOR closestHurt_{};
        ClosestPointsOnSegments(
            hitCapsule_.p0,
            hitCapsule_.p1,
            hurtCapsule_.p0,
            hurtCapsule_.p1,
            closestHit_,
            closestHurt_);

        XMVECTOR normal_ = manifold_.normal;
        if (XMVectorGetX(XMVector3LengthSq(normal_)) <= 0.000001f)
        {
            normal_ = XMVectorSubtract(closestHurt_, closestHit_);
        }
        if (XMVectorGetX(XMVector3LengthSq(normal_)) <= 0.000001f)
        {
            normal_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
        normal_ = XMVector3Normalize(normal_);

        const XMVECTOR hitSurface_ = XMVectorAdd(
            closestHit_,
            XMVectorScale(normal_, hitCapsule_.radius));
        const XMVECTOR hurtSurface_ = XMVectorSubtract(
            closestHurt_,
            XMVectorScale(normal_, hurtCapsule_.radius));
        const XMVECTOR position_ = XMVectorScale(
            XMVectorAdd(hitSurface_, hurtSurface_),
            0.5f);

        AttackHitContact contact_{};
        XMStoreFloat3(&contact_.position, position_);
        XMStoreFloat3(&contact_.normal, normal_);
        contact_.isValid = true;
        return contact_;
    }
}

void CollisionStepSystem::RegisterCollider(
    const std::shared_ptr<ColliderComponent>& collider_)
{
    if (!collider_) return;

    RemoveExpiredColliders();
    const bool registered_ = std::any_of(
        colliders.begin(),
        colliders.end(),
        [&collider_](const std::weak_ptr<ColliderComponent>& registeredCollider_)
        {
            return registeredCollider_.lock().get() == collider_.get();
        });
    if (!registered_) colliders.emplace_back(collider_);
}

void CollisionStepSystem::UnregisterCollider(ColliderComponent* collider_)
{
    if (!collider_) return;

    const std::shared_ptr<GameObject> removedObject_ = collider_->GetGameObject();
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        const std::shared_ptr<ColliderComponent> currentCollider_ = registeredCollider_.lock();
        if (!currentCollider_) continue;

        if (removedObject_) currentCollider_->currentTriggers.erase(removedObject_);
    }
    collider_->currentTriggers.clear();

    colliders.erase(
        std::remove_if(
            colliders.begin(),
            colliders.end(),
            [collider_](const std::weak_ptr<ColliderComponent>& registeredCollider_)
            {
                const std::shared_ptr<ColliderComponent> currentCollider_ =
                    registeredCollider_.lock();
                return !currentCollider_ || currentCollider_.get() == collider_;
            }),
        colliders.end());
}

void CollisionStepSystem::Clear()
{
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        if (const std::shared_ptr<ColliderComponent> collider_ = registeredCollider_.lock())
        {
            collider_->currentTriggers.clear();
        }
    }
    colliders.clear();
}

void CollisionStepSystem::SyncFromPhysics(float deltaTime_)
{
    RemoveExpiredColliders();
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        if (const std::shared_ptr<ColliderComponent> collider_ = registeredCollider_.lock())
        {
            collider_->SyncFromPhysics(deltaTime_);
        }
    }
}

void CollisionStepSystem::Step(float deltaTime_)
{
    RemoveExpiredColliders();
    PhysXSystem::ComputeCharacterControllerInteractions(deltaTime_);
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        const std::shared_ptr<ColliderComponent> collider_ = registeredCollider_.lock();
        if (!collider_) continue;

        collider_->isGrounded = false;
        collider_->SetHitFlag(false);
        collider_->PreparePhysicsStep(deltaTime_);
    }

    StepCombatPairs();
}

void CollisionStepSystem::HandlePhysXTrigger(
    ColliderComponent* colliderA_,
    ColliderComponent* colliderB_,
    bool hit_)
{
    HandleTriggerPair(colliderA_, colliderB_, hit_, nullptr);
}

void CollisionStepSystem::HandleTriggerPair(
    ColliderComponent* colliderA_,
    ColliderComponent* colliderB_,
    bool hit_,
    const AttackHitContact* contact_)
{
    if (!colliderA_ || !colliderB_) return;

    const std::shared_ptr<GameObject> objectA_ = colliderA_->GetGameObject();
    const std::shared_ptr<GameObject> objectB_ = colliderB_->GetGameObject();
    if (!objectA_ || !objectB_) return;

    const bool existsA_ = colliderA_->currentTriggers.count(objectB_) != 0;
    const bool existsB_ = colliderB_->currentTriggers.count(objectA_) != 0;
    if (hit_)
    {
        if (!existsA_)
        {
            colliderA_->currentTriggers.insert(objectB_);
            colliderA_->OnTriggerEnter(objectB_);
            for (const ColliderComponent::CollisionCallback& callback_ :
                colliderA_->onTriggerEnterCallbacks)
            {
                if (callback_) callback_(objectB_);
            }
        }
        else
        {
            colliderA_->OnTriggerStay(objectB_);
        }

        if (!existsB_)
        {
            colliderB_->currentTriggers.insert(objectA_);
            colliderB_->OnTriggerEnter(objectA_);
            for (const ColliderComponent::CollisionCallback& callback_ :
                colliderB_->onTriggerEnterCallbacks)
            {
                if (callback_) callback_(objectA_);
            }
        }
        else
        {
            colliderB_->OnTriggerStay(objectA_);
        }

        HitBox* hitBox_ = dynamic_cast<HitBox*>(colliderA_);
        HurtBox* hurtBox_ = dynamic_cast<HurtBox*>(colliderB_);
        if (!hitBox_ || !hurtBox_)
        {
            hitBox_ = dynamic_cast<HitBox*>(colliderB_);
            hurtBox_ = dynamic_cast<HurtBox*>(colliderA_);
        }

        if (attackSystem && hitBox_ && hurtBox_)
        {
            if (contact_ && contact_->isValid)
            {
                attackSystem->HandleHit(*hitBox_, *hurtBox_, *contact_);
            }
            else
            {
                attackSystem->HandleHit(*hitBox_, *hurtBox_);
            }
        }
        return;
    }

    if (existsA_)
    {
        colliderA_->currentTriggers.erase(objectB_);
        colliderA_->OnTriggerExit(objectB_);
    }
    if (existsB_)
    {
        colliderB_->currentTriggers.erase(objectA_);
        colliderB_->OnTriggerExit(objectA_);
    }
}

void CollisionStepSystem::HandlePhysXContact(
    ColliderComponent* colliderA_,
    ColliderComponent* colliderB_,
    const CollisionContact& contactA_,
    const CollisionContact& contactB_)
{
    if (!colliderA_ || !colliderB_) return;

    const std::shared_ptr<GameObject> objectA_ = colliderA_->GetGameObject();
    const std::shared_ptr<GameObject> objectB_ = colliderB_->GetGameObject();
    if (!objectA_ || !objectB_) return;

    colliderA_->OnCollisionEnter(objectB_);
    colliderB_->OnCollisionEnter(objectA_);
    for (const ColliderComponent::CollisionContactCallback& callback_ :
        colliderA_->onCollisionContactCallbacks)
    {
        if (callback_) callback_(objectB_, contactA_);
    }
    for (const ColliderComponent::CollisionContactCallback& callback_ :
        colliderB_->onCollisionContactCallbacks)
    {
        if (callback_) callback_(objectA_, contactB_);
    }
}

void CollisionStepSystem::RemoveExpiredColliders()
{
    colliders.erase(
        std::remove_if(
            colliders.begin(),
            colliders.end(),
            [](const std::weak_ptr<ColliderComponent>& collider_)
            {
                return collider_.expired();
            }),
        colliders.end());
}

void CollisionStepSystem::StepCombatPairs()
{
    // Combat volumes are also checked here so gameplay hits do not depend on
    // PhysX contact-report availability for kinematic attachment actors.
    std::vector<std::shared_ptr<HitBox>> hitBoxes_;
    std::vector<std::shared_ptr<HurtBox>> hurtBoxes_;
    std::vector<std::shared_ptr<JustDodgeCollider>> justDodgeColliders_;

    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        const std::shared_ptr<ColliderComponent> collider_ =
            registeredCollider_.lock();
        if (!collider_) continue;

        if (const std::shared_ptr<HitBox> hitBox_ =
            std::dynamic_pointer_cast<HitBox>(collider_))
        {
            hitBoxes_.push_back(hitBox_);
            continue;
        }
        if (const std::shared_ptr<HurtBox> hurtBox_ =
            std::dynamic_pointer_cast<HurtBox>(collider_))
        {
            hurtBoxes_.push_back(hurtBox_);
        }
        if (const std::shared_ptr<JustDodgeCollider> justDodgeCollider_ =
            std::dynamic_pointer_cast<JustDodgeCollider>(collider_))
        {
            justDodgeColliders_.push_back(justDodgeCollider_);
            continue;
        }
    }

    std::vector<Capsule> hitCapsules_(hitBoxes_.size());
    std::vector<Capsule> hurtCapsules_(hurtBoxes_.size());
    std::vector<Capsule> justDodgeCapsules_(justDodgeColliders_.size());

    for (size_t hitIndex_ = 0; hitIndex_ < hitBoxes_.size(); ++hitIndex_)
    {
        if (hitBoxes_[hitIndex_]->IsEnabled())
        {
            hitCapsules_[hitIndex_] = hitBoxes_[hitIndex_]->GetCapsuleData();
        }
    }
    for (size_t hurtIndex_ = 0; hurtIndex_ < hurtBoxes_.size(); ++hurtIndex_)
    {
        if (hurtBoxes_[hurtIndex_]->IsEnabled())
        {
            hurtCapsules_[hurtIndex_] = hurtBoxes_[hurtIndex_]->GetCapsuleData();
        }
    }

    for (size_t dodgeIndex_ = 0; dodgeIndex_ < justDodgeColliders_.size(); ++dodgeIndex_)
    {
        if (justDodgeColliders_[dodgeIndex_]->IsEnabled())
        {
            justDodgeCapsules_[dodgeIndex_] =
                justDodgeColliders_[dodgeIndex_]->GetCapsuleData();
        }
    }

    for (size_t hitIndex_ = 0; hitIndex_ < hitBoxes_.size(); ++hitIndex_)
    {
        const std::shared_ptr<HitBox>& hitBox_ = hitBoxes_[hitIndex_];
        for (size_t hurtIndex_ = 0; hurtIndex_ < hurtBoxes_.size(); ++hurtIndex_)
        {
            const std::shared_ptr<HurtBox>& hurtBox_ = hurtBoxes_[hurtIndex_];
            bool isOverlapping_ = false;
            AttackHitContact contact_{};
            if (hitBox_->IsEnabled() && hurtBox_->IsEnabled())
            {
                const CollisionManifold manifold_ = CollisionMath::CapsuleVsCapsule(
                    hitCapsules_[hitIndex_],
                    hurtCapsules_[hurtIndex_]);
                isOverlapping_ = manifold_.isHit;
                if (isOverlapping_)
                {
                    contact_ = MakeAttackHitContact(
                        hitCapsules_[hitIndex_],
                        hurtCapsules_[hurtIndex_],
                        manifold_);
                }
            }

            HandleTriggerPair(
                hitBox_.get(),
                hurtBox_.get(),
                isOverlapping_,
                contact_.isValid ? &contact_ : nullptr);
        }
    }

    for (size_t hitIndex_ = 0; hitIndex_ < hitBoxes_.size(); ++hitIndex_)
    {
        const std::shared_ptr<HitBox>& hitBox_ = hitBoxes_[hitIndex_];
        if (!hitBox_->IsEnabled()) continue;

        for (size_t dodgeIndex_ = 0; dodgeIndex_ < justDodgeColliders_.size(); ++dodgeIndex_)
        {
            const std::shared_ptr<JustDodgeCollider>& justDodgeCollider_ =
                justDodgeColliders_[dodgeIndex_];

            if (!justDodgeCollider_->IsEnabled()) continue;

            const CollisionManifold manifold_ = CollisionMath::CapsuleVsCapsule(
                hitCapsules_[hitIndex_],
                justDodgeCapsules_[dodgeIndex_]);

            if (!manifold_.isHit) continue;

            if (justDodgeSystem)
            {
                JustDodgeContact contact_{};
                contact_.normal = { 0.0f, 1.0f, 0.0f };
                contact_.position = { 0.0f, 0.0f, 0.0f };
                contact_.isValid = false;

                justDodgeSystem->HandleJustDodge(
                    *hitBox_,
                    *justDodgeCollider_,
                    contact_);
            }
        }
    }

}
