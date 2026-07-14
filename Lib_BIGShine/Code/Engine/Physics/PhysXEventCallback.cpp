#include "PhysXEventCallback.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "ColliderComponent.h"

using namespace physx;

namespace
{
bool IsMaskOnlyPair(ColliderComponent* colA_, ColliderComponent* colB_)
{
    auto objA_ = colA_ ? colA_->GetGameObject() : nullptr;
    auto objB_ = colB_ ? colB_->GetGameObject() : nullptr;
    if (!objA_ || !objB_) return false;

    return (objA_->mask & static_cast<uint32_t>(objB_->layer)) != 0
        || (objB_->mask & static_cast<uint32_t>(objA_->layer)) != 0;
}

bool IsManualCombatRole(ColliderComponent* collider_)
{
    if (!collider_) return false;

    const ColliderComponent::CollisionRole role_ = collider_->GetCollisionRole();
    return role_ == ColliderComponent::CollisionRole::HIT_BOX ||
        role_ == ColliderComponent::CollisionRole::HURT_BOX ||
        role_ == ColliderComponent::CollisionRole::JUST_DODGE_BOX ||
        role_ == ColliderComponent::CollisionRole::GUARD_BOX;
}

bool IsDetectionOnlyPair(ColliderComponent* colA_, ColliderComponent* colB_)
{
    return !colA_->UsesPhysicsResponse() || !colB_->UsesPhysicsResponse()
        || colA_->GetCollisionRole() == ColliderComponent::CollisionRole::SENSOR
        || colB_->GetCollisionRole() == ColliderComponent::CollisionRole::SENSOR;
}
}

void CollisionCallback::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
    for (PxU32 i_ = 0; i_ < count; ++i_)
    {
        PxTriggerPair& pair_ = pairs[i_];
        if (pair_.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
        {
            continue;
        }

        auto colA_ = reinterpret_cast<ColliderComponent*>(pair_.triggerActor ? pair_.triggerActor->userData : nullptr);
        auto colB_ = reinterpret_cast<ColliderComponent*>(pair_.otherActor ? pair_.otherActor->userData : nullptr);
        if (!colA_ || !colB_) continue;
        if (IsManualCombatRole(colA_) || IsManualCombatRole(colB_)) continue;

        const bool hit_ = pair_.status != PxPairFlag::eNOTIFY_TOUCH_LOST;
        colA_->GetManager().GetCollisionStepSystem().HandlePhysXTrigger(colA_, colB_, hit_);
    }
}

void CollisionCallback::onContact(const PxContactPairHeader& header, const PxContactPair* pairs, PxU32 nbPairs)
{
    auto colA_ = reinterpret_cast<ColliderComponent*>(header.actors[0] ? header.actors[0]->userData : nullptr);
    auto colB_ = reinterpret_cast<ColliderComponent*>(header.actors[1] ? header.actors[1]->userData : nullptr);
    if (!colA_ || !colB_) return;
    if (IsManualCombatRole(colA_) || IsManualCombatRole(colB_)) return;
    // header.actors[] can point to actors that were removed from the scene.
    for (PxU32 i_ = 0; i_ < nbPairs; ++i_)
    {
        const PxContactPair& pair_ = pairs[i_];
        const bool lost_ = pair_.events & PxPairFlag::eNOTIFY_TOUCH_LOST;
        const bool touching_ = pair_.events & (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS);

        if (IsMaskOnlyPair(colA_, colB_) || IsDetectionOnlyPair(colA_, colB_))
        {
            if (touching_ || lost_)
            {
                colA_->GetManager().GetCollisionStepSystem().HandlePhysXTrigger(colA_, colB_, !lost_);
            }
            continue;
        }

        if (!touching_) continue;

        colA_->SetHitFlag(true);
        colB_->SetHitFlag(true);

        PxContactPairPoint contacts_[8];
        const PxU32 contactCount_ = pair_.extractContacts(contacts_, 8);
        CollisionContact contactA_{};
        CollisionContact contactB_{};
        if (contactCount_ > 0)
        {
            const PxContactPairPoint& point_ = contacts_[0];
            contactA_.position = { point_.position.x, point_.position.y, point_.position.z };
            contactA_.normal = { point_.normal.x, point_.normal.y, point_.normal.z };
            contactA_.isValid = true;
            contactB_.position = contactA_.position;
            contactB_.normal = {
                -contactA_.normal.x,
                -contactA_.normal.y,
                -contactA_.normal.z };
            contactB_.isValid = true;
        }
        colA_->GetManager().GetCollisionStepSystem().HandlePhysXContact(
            colA_,
            colB_,
            contactA_,
            contactB_);
        for (PxU32 contactIndex_ = 0; contactIndex_ < contactCount_; ++contactIndex_)
        {
            const float normalY_ = contacts_[contactIndex_].normal.y;
            if (normalY_ > 0.5f && !colB_->isStatic) colB_->isGrounded = true;
            if (normalY_ < -0.5f && !colA_->isStatic) colA_->isGrounded = true;
        }
    }
}
