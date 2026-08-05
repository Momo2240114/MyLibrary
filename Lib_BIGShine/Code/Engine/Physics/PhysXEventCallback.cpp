#include "PrecompiledHeader.h"

#include "PhysXEventCallback.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "ColliderComponent.h"

using namespace physx;

namespace
{
bool IsMaskOnlyPair(ColliderComponent* colA_, ColliderComponent* colB_)
{
    // Layer/Mask上は検知したいが、物理押し返しはしない組み合わせかを判定する。
    auto objA_ = colA_ ? colA_->GetGameObject() : nullptr;
    auto objB_ = colB_ ? colB_->GetGameObject() : nullptr;
    if (!objA_ || !objB_) return false;

    return (objA_->mask & static_cast<uint32_t>(objB_->layer)) != 0
        || (objB_->mask & static_cast<uint32_t>(objA_->layer)) != 0;
}

bool IsManualCombatRole(ColliderComponent* collider_)
{
    if (!collider_) return false;

    // Hit/Hurt/Guard/JustDodgeはCombat側の手動判定で扱うため、PhysX通知では処理しない。
    const ColliderComponent::CollisionRole role_ = collider_->GetCollisionRole();
    return role_ == ColliderComponent::CollisionRole::HIT_BOX ||
        role_ == ColliderComponent::CollisionRole::HURT_BOX ||
        role_ == ColliderComponent::CollisionRole::JUST_DODGE_BOX ||
        role_ == ColliderComponent::CollisionRole::GUARD_BOX;
}

bool IsDetectionOnlyPair(ColliderComponent* colA_, ColliderComponent* colB_)
{
    // Sensorや物理応答なしColliderは、ContactではなくTrigger相当の通知として扱う。
    return !colA_->UsesPhysicsResponse() || !colB_->UsesPhysicsResponse()
        || colA_->GetCollisionRole() == ColliderComponent::CollisionRole::SENSOR
        || colB_->GetCollisionRole() == ColliderComponent::CollisionRole::SENSOR;
}
}

void CollisionCallback::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
    // PhysX Trigger通知をCollisionStepSystemへ渡し、Enter/Exit相当の管理をエンジン側へ集約する。
    for (PxU32 i_ = 0; i_ < count; ++i_)
    {
        PxTriggerPair& pair_ = pairs[i_];
        if (pair_.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
        {
            continue;
        }

        auto colA_ = reinterpret_cast<ColliderComponent*>(pair_.triggerActor ? pair_.triggerActor->userData : nullptr);
        auto colB_ = reinterpret_cast<ColliderComponent*>(pair_.otherActor ? pair_.otherActor->userData : nullptr);
        // userDataにはColliderComponentを入れているため、PhysX Actorからエンジン側Colliderへ戻す。
        if (!colA_ || !colB_) continue;
        if (IsManualCombatRole(colA_) || IsManualCombatRole(colB_)) continue;

        const bool hit_ = pair_.status != PxPairFlag::eNOTIFY_TOUCH_LOST;
        colA_->GetManager().GetCollisionStepSystem().HandlePhysXTrigger(colA_, colB_, hit_);
    }
}

void CollisionCallback::onContact(const PxContactPairHeader& header, const PxContactPair* pairs, PxU32 nbPairs)
{
    // 物理Contact通知をColliderComponentへ戻し、接触点や接地判定をエンジン側へ反映する。
    auto colA_ = reinterpret_cast<ColliderComponent*>(header.actors[0] ? header.actors[0]->userData : nullptr);
    auto colB_ = reinterpret_cast<ColliderComponent*>(header.actors[1] ? header.actors[1]->userData : nullptr);
    if (!colA_ || !colB_) return;
    if (IsManualCombatRole(colA_) || IsManualCombatRole(colB_)) return;
    // header.actors[]はSceneから削除済みActorを指すことがあるため、各Pairの内容も確認する。
    for (PxU32 i_ = 0; i_ < nbPairs; ++i_)
    {
        const PxContactPair& pair_ = pairs[i_];
        const bool lost_ = pair_.events & PxPairFlag::eNOTIFY_TOUCH_LOST;
        const bool touching_ = pair_.events & (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS);

        if (IsMaskOnlyPair(colA_, colB_) || IsDetectionOnlyPair(colA_, colB_))
        {
            // 検知専用の組み合わせは物理接触として押し返さず、Triggerイベントへ変換する。
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
        // 代表接触点をA/B双方の向きで持ち、Hit/Collision処理が同じ情報を読めるようにする。
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
            // 法線の上下方向から、キャラクターが地面として扱えるColliderに乗っているかを判定する。
            const float normalY_ = contacts_[contactIndex_].normal.y;
            if (normalY_ > 0.5f &&
                !colB_->isStatic &&
                colA_->CanBeCharacterGround())
            {
                colB_->isGrounded = true;
            }
            if (normalY_ < -0.5f &&
                !colA_->isStatic &&
                colB_->CanBeCharacterGround())
            {
                colA_->isGrounded = true;
            }
        }
    }
}
