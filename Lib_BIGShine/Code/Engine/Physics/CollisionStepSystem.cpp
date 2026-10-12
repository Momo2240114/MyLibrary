#include "PrecompiledHeader.h"

#include "CollisionStepSystem.h"

#include "ColliderComponent.h"
#include "HitBox.h"
#include "HurtBox.h"
#include "GuardCollider.h"
#include "PhysXSystem.h"
#include "Engine/GamePlay/System/GuardSystem.h"
#include "Engine/GamePlay/System/JustDodgeSystem.h"
#include "Engine/GamePlay/System/JustGuardSystem.h"
#include "Engine/Physics/JustDodgeCollider.h"


#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Math/CollisionMath.h"
#include "Engine/Debug/TracyProfiler.h"


#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>

namespace
{
    struct SuppressedHitTarget
    {
        HitBox* hitBox = nullptr;
        GameObject* defenderRoot = nullptr;
    };

    float Clamp01(float value_)
    {
        return (std::max)(0.0f, (std::min)(value_, 1.0f));
    }

    // 2つの線分上で最も近い点を求め、Capsule同士の接触点計算に使う。
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
        // HitBoxとHurtBoxのCapsule表面の中間点を、攻撃Hitの接触位置として作る。
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

    GuardContact MakeGuardContact(
        const Capsule& hitCapsule_,
        const Capsule& guardCapsule_,
        const CollisionManifold& manifold_)
    {
        // HitBoxとGuardColliderのCapsule表面の中間点を、Guard成立位置として作る。
        using namespace DirectX;

        XMVECTOR closestHit_{};
        XMVECTOR closestGuard_{};
        ClosestPointsOnSegments(
            hitCapsule_.p0,
            hitCapsule_.p1,
            guardCapsule_.p0,
            guardCapsule_.p1,
            closestHit_,
            closestGuard_);

        XMVECTOR normal_ = manifold_.normal;
        if (XMVectorGetX(XMVector3LengthSq(normal_)) <= 0.000001f)
        {
            normal_ = XMVectorSubtract(closestGuard_, closestHit_);
        }
        if (XMVectorGetX(XMVector3LengthSq(normal_)) <= 0.000001f)
        {
            normal_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
        normal_ = XMVector3Normalize(normal_);

        const XMVECTOR hitSurface_ = XMVectorAdd(
            closestHit_,
            XMVectorScale(normal_, hitCapsule_.radius));
        const XMVECTOR guardSurface_ = XMVectorSubtract(
            closestGuard_,
            XMVectorScale(normal_, guardCapsule_.radius));
        const XMVECTOR position_ = XMVectorScale(
            XMVectorAdd(hitSurface_, guardSurface_),
            0.5f);

        GuardContact contact_{};
        XMStoreFloat3(&contact_.position, position_);
        XMStoreFloat3(&contact_.normal, normal_);
        contact_.isValid = true;
        return contact_;
    }

    bool IsCombatOnlyRole(ColliderComponent::CollisionRole role_)
    {
        // Gameplay用Volume同士は、許可された組み合わせだけTriggerとして扱う。
        return role_ == ColliderComponent::CollisionRole::HIT_BOX ||
            role_ == ColliderComponent::CollisionRole::HURT_BOX ||
            role_ == ColliderComponent::CollisionRole::JUST_DODGE_BOX ||
            role_ == ColliderComponent::CollisionRole::GUARD_BOX;
    }

    bool IsAllowedCombatTriggerPair(ColliderComponent* colliderA_, ColliderComponent* colliderB_)
    {
        if (!colliderA_ || !colliderB_) return false;

        const ColliderComponent::CollisionRole roleA_ = colliderA_->GetCollisionRole();
        const ColliderComponent::CollisionRole roleB_ = colliderB_->GetCollisionRole();
        if (!IsCombatOnlyRole(roleA_) && !IsCombatOnlyRole(roleB_)) return true;

        const bool hitHurtPair_ =
            (roleA_ == ColliderComponent::CollisionRole::HIT_BOX &&
                roleB_ == ColliderComponent::CollisionRole::HURT_BOX) ||
            (roleB_ == ColliderComponent::CollisionRole::HIT_BOX &&
                roleA_ == ColliderComponent::CollisionRole::HURT_BOX);
        const bool hitJustDodgePair_ =
            (roleA_ == ColliderComponent::CollisionRole::HIT_BOX &&
                roleB_ == ColliderComponent::CollisionRole::JUST_DODGE_BOX) ||
            (roleB_ == ColliderComponent::CollisionRole::HIT_BOX &&
                roleA_ == ColliderComponent::CollisionRole::JUST_DODGE_BOX);
        const bool hitGuardPair_ =
            (roleA_ == ColliderComponent::CollisionRole::HIT_BOX &&
                roleB_ == ColliderComponent::CollisionRole::GUARD_BOX) ||
            (roleB_ == ColliderComponent::CollisionRole::HIT_BOX &&
                roleA_ == ColliderComponent::CollisionRole::GUARD_BOX);

        return hitHurtPair_ || hitJustDodgePair_ || hitGuardPair_;
    }

    bool ShouldSuppressTriggerPair(ColliderComponent* colliderA_, ColliderComponent* colliderB_)
    {
        // 同じ所有者内の判定や、Combat用途として不正な組み合わせは通知しない。
        if (!colliderA_ || !colliderB_) return true;
        const uint32_t ownerIdA_ = colliderA_->GetCollisionOwnerId();
        const uint32_t ownerIdB_ = colliderB_->GetCollisionOwnerId();
        return (ownerIdA_ != 0u && ownerIdB_ != 0u && ownerIdA_ == ownerIdB_) ||
            !IsAllowedCombatTriggerPair(colliderA_, colliderB_);
    }

    bool CanStepCollider(const std::shared_ptr<ColliderComponent>& collider_)
    {
        // Load未完了や無効Colliderは、このFrameの判定対象から外す。
        if (!collider_ || !collider_->IsEnabled()) return false;
        if (!collider_->IsStarted()) return false;

        collider_->UpdateLoadState();
        return collider_->IsLoadReady();
    }

    std::shared_ptr<GameObject> GetHierarchyRoot(std::shared_ptr<GameObject> object_)
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

    bool IsHitSuppressedForTarget(
        HitBox* hitBox_,
        HurtBox* hurtBox_,
        const std::vector<SuppressedHitTarget>& suppressedTargets_)
    {
        if (!hitBox_ || !hurtBox_) return false;

        const std::shared_ptr<GameObject> hurtObject_ = hurtBox_->GetGameObject();
        const std::shared_ptr<GameObject> hurtRoot_ = GetHierarchyRoot(hurtObject_);
        if (!hurtRoot_) return false;

        for (const SuppressedHitTarget& suppressedTarget_ : suppressedTargets_)
        {
            if (suppressedTarget_.hitBox == hitBox_ &&
                suppressedTarget_.defenderRoot == hurtRoot_.get())
            {
                return true;
            }
        }

        return false;
    }

    void AddSuppressedHitTarget(
        HitBox* hitBox_,
        const std::shared_ptr<GameObject>& defender_,
        std::vector<SuppressedHitTarget>& suppressedTargets_)
    {
        // Guard/JustGuard済みの防御者階層だけ、後段HurtBoxへのHit通知を止める。
        if (!hitBox_ || !defender_) return;

        const std::shared_ptr<GameObject> defenderRoot_ =
            GetHierarchyRoot(defender_);
        if (!defenderRoot_) return;

        suppressedTargets_.push_back(
            { hitBox_, defenderRoot_.get() });
    }
}

void CollisionStepSystem::RegisterCollider(
    const std::shared_ptr<ColliderComponent>& collider_)
{
    // 同じColliderを二重登録しないよう確認してから弱参照で保持する。
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
    // Collider削除時は、他Colliderが持つTrigger中情報からも対象Objectを外す。
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

std::vector<std::shared_ptr<HitBox>> CollisionStepSystem::CollectActiveHitBoxes()
{
    // 弱参照を掃除してから、有効なHitBoxだけを収集する。
    RemoveExpiredColliders();

    std::vector<std::shared_ptr<HitBox>> hitBoxes_;
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        const std::shared_ptr<ColliderComponent> collider_ =
            registeredCollider_.lock();
        if (!CanStepCollider(collider_)) continue;

        if (const std::shared_ptr<HitBox> hitBox_ =
            std::dynamic_pointer_cast<HitBox>(collider_))
        {
            hitBoxes_.push_back(hitBox_);
        }
    }
    return hitBoxes_;
}

void CollisionStepSystem::SyncFromPhysics(float deltaTime_)
{
    // PhysXで動いた位置や接地状態を、各ColliderComponentへ反映する。
    RemoveExpiredColliders();
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        if (const std::shared_ptr<ColliderComponent> collider_ = registeredCollider_.lock())
        {
            if (!CanStepCollider(collider_)) continue;
            collider_->SyncFromPhysics(deltaTime_);
        }
    }
}

void CollisionStepSystem::PrepareInitialPhysicsState(float deltaTime_)
{
    RemoveExpiredColliders();
    for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
    {
        if (const std::shared_ptr<ColliderComponent> collider_ = registeredCollider_.lock())
        {
            if (!CanStepCollider(collider_)) continue;
            collider_->PreparePhysicsStep(deltaTime_);
        }
    }
}

void CollisionStepSystem::Step(float deltaTime_)
{
    BS_PROFILE_SCOPE("CollisionStepSystem::Step");
    {
        BS_PROFILE_SCOPE("CollisionStepSystem::CollectColliders");
        RemoveExpiredColliders();
        BS_PROFILE_PLOT("Collision.ColliderCount", static_cast<int64_t>(colliders.size()));
    }
    {
        BS_PROFILE_SCOPE("PhysX::CharacterControllerMove");
        PhysXSystem::ComputeCharacterControllerInteractions(deltaTime_);
    }
    {
        BS_PROFILE_SCOPE("CollisionStepSystem::PreparePhysicsStep");
        // 判定前にFrame用Flagを初期化し、ColliderごとのPhysics準備を行う。
        for (const std::weak_ptr<ColliderComponent>& registeredCollider_ : colliders)
        {
            const std::shared_ptr<ColliderComponent> collider_ = registeredCollider_.lock();
            if (!CanStepCollider(collider_)) continue;

            collider_->isGrounded = false;
            collider_->SetHitFlag(false);
            collider_->PreparePhysicsStep(deltaTime_);
        }
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::StepCombatPairs");
        StepCombatPairs();
    }
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
    // Trigger Enter/Stay/Exitを更新し、HitBox/HurtBoxならAttackSystemへ通知する。
    if (!colliderA_ || !colliderB_) return;

    const std::shared_ptr<GameObject> objectA_ = colliderA_->GetGameObject();
    const std::shared_ptr<GameObject> objectB_ = colliderB_->GetGameObject();
    if (!objectA_ || !objectB_) return;

    const bool existsA_ = colliderA_->currentTriggers.count(objectB_) != 0;
    const bool existsB_ = colliderB_->currentTriggers.count(objectA_) != 0;
    if (ShouldSuppressTriggerPair(colliderA_, colliderB_))
    {
        hit_ = false;
        contact_ = nullptr;
    }

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
    // PhysXから渡された接触を、両ColliderのCollision callbackへ橋渡しする。
    if (!colliderA_ || !colliderB_) return;

    const std::shared_ptr<GameObject> objectA_ = colliderA_->GetGameObject();
    const std::shared_ptr<GameObject> objectB_ = colliderB_->GetGameObject();
    if (!objectA_ || !objectB_) return;
    if (colliderA_->GetCollisionOwnerId() == colliderB_->GetCollisionOwnerId())
    {
        return;
    }

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
    // GameObject削除などで所有元が消えたCollider参照を掃除する。
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
    // HitBox/HurtBox/JustDodge/GuardはGameplay用Volumeなので、PhysXではなく手動で組み合わせ判定する。
    std::vector<std::shared_ptr<HitBox>> hitBoxes_;
    std::vector<std::shared_ptr<HurtBox>> hurtBoxes_;
    std::vector<std::shared_ptr<JustDodgeCollider>> justDodgeColliders_;
    std::vector<std::shared_ptr<GuardCollider>> guardColliders_;
    std::vector<SuppressedHitTarget> suppressedHitTargets_;

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::BroadPhase");
        // Collider型ごとに配列を分け、後段の組み合わせ判定を単純化する。
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
            if (const std::shared_ptr<GuardCollider> guardCollider_ =
                std::dynamic_pointer_cast<GuardCollider>(collider_))
            {
                guardColliders_.push_back(guardCollider_);
                continue;
            }
        }
    }

    BS_PROFILE_PLOT("Collision.HitBoxCount", static_cast<int64_t>(hitBoxes_.size()));
    BS_PROFILE_PLOT("Collision.HurtBoxCount", static_cast<int64_t>(hurtBoxes_.size()));
    BS_PROFILE_PLOT("Collision.GuardCheckCount", static_cast<int64_t>(guardColliders_.size()));

    std::vector<Capsule> hitCapsules_(hitBoxes_.size());
    std::vector<Capsule> hurtCapsules_(hurtBoxes_.size());
    std::vector<Capsule> justDodgeCapsules_(justDodgeColliders_.size());
    std::vector<Capsule> guardCapsules_(guardColliders_.size());

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

    for (size_t guardIndex_ = 0; guardIndex_ < guardColliders_.size(); ++guardIndex_)
    {
        if (guardColliders_[guardIndex_]->IsEnabled())
        {
            guardCapsules_[guardIndex_] =
                guardColliders_[guardIndex_]->GetCapsuleData();
        }
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::SensorCollision");
        // HitBoxとJustDodgeColliderの重なりを先に見て、回避成功を通知する。
        for (size_t hitIndex_ = 0; hitIndex_ < hitBoxes_.size(); ++hitIndex_)
        {
            const std::shared_ptr<HitBox>& hitBox_ = hitBoxes_[hitIndex_];
            for (size_t dodgeIndex_ = 0; dodgeIndex_ < justDodgeColliders_.size(); ++dodgeIndex_)
            {
                const std::shared_ptr<JustDodgeCollider>& justDodgeCollider_ =
                    justDodgeColliders_[dodgeIndex_];

                bool isOverlapping_ = false;
                if (hitBox_->IsEnabled() && justDodgeCollider_->IsEnabled())
                {
                    const CollisionManifold manifold_ = CollisionMath::CapsuleVsCapsule(
                        hitCapsules_[hitIndex_],
                        justDodgeCapsules_[dodgeIndex_]);
                    isOverlapping_ = manifold_.isHit;
                }

                HandleTriggerPair(
                    hitBox_.get(),
                    justDodgeCollider_.get(),
                    isOverlapping_,
                    nullptr);

                if (isOverlapping_ &&
                    !ShouldSuppressTriggerPair(hitBox_.get(), justDodgeCollider_.get()) &&
                    justDodgeSystem)
                {
                    BS_LOG_INFO(
                        LOG_CATEGORY::PHYSICS,
                        std::string("[JustDodge] CollisionStepSystem overlap. HitBox=")
                        + hitBox_->GetName()
                        + " JustDodgeCollider="
                        + justDodgeCollider_->GetName());

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

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::GuardCollision");
        // Guard/JustGuardが成立した防御者には、同じHitBoxを後段のHurtBox Hitへ流さない。
        for (size_t hitIndex_ = 0; hitIndex_ < hitBoxes_.size(); ++hitIndex_)
        {
            const std::shared_ptr<HitBox>& hitBox_ = hitBoxes_[hitIndex_];
            for (size_t guardIndex_ = 0; guardIndex_ < guardColliders_.size(); ++guardIndex_)
            {
                const std::shared_ptr<GuardCollider>& guardCollider_ =
                    guardColliders_[guardIndex_];

                bool isOverlapping_ = false;
                GuardContact contact_{};
                if (hitBox_->IsEnabled() && guardCollider_->IsEnabled())
                {
                    const CollisionManifold manifold_ = CollisionMath::CapsuleVsCapsule(
                        hitCapsules_[hitIndex_],
                        guardCapsules_[guardIndex_]);
                    isOverlapping_ = manifold_.isHit;
                    if (isOverlapping_)
                    {
                        contact_ = MakeGuardContact(
                            hitCapsules_[hitIndex_],
                            guardCapsules_[guardIndex_],
                            manifold_);
                    }
                }

                HandleTriggerPair(
                    hitBox_.get(),
                    guardCollider_.get(),
                    isOverlapping_,
                    nullptr);

                if (isOverlapping_ &&
                    !ShouldSuppressTriggerPair(hitBox_.get(), guardCollider_.get()) &&
                    guardCollider_->IsJustGuardEnabled() &&
                    justGuardSystem)
                {
                    JustGuardContact justGuardContact_{};
                    justGuardContact_.position = contact_.position;
                    justGuardContact_.normal = contact_.normal;
                    justGuardContact_.isValid = contact_.isValid;

                    const bool justGuarded_ = justGuardSystem->HandleJustGuard(
                        *hitBox_,
                        *guardCollider_,
                        justGuardContact_);
                    if (justGuarded_)
                    {
                        AddSuppressedHitTarget(
                            hitBox_.get(),
                            guardCollider_->GetGuardOwner(),
                            suppressedHitTargets_);
                        break;
                    }
                    if (guardCollider_->HasGuardedAttackRecord(*hitBox_))
                    {
                        AddSuppressedHitTarget(
                            hitBox_.get(),
                            guardCollider_->GetGuardOwner(),
                            suppressedHitTargets_);
                        break;
                    }
                }

                if (isOverlapping_ &&
                    !ShouldSuppressTriggerPair(hitBox_.get(), guardCollider_.get()) &&
                    guardSystem)
                {
                    const bool guarded_ = guardSystem->HandleGuard(
                        *hitBox_,
                        *guardCollider_,
                        contact_);
                    if (guarded_)
                    {
                        AddSuppressedHitTarget(
                            hitBox_.get(),
                            guardCollider_->GetGuardOwner(),
                            suppressedHitTargets_);
                        break;
                    }
                    if (guardCollider_->HasGuardedAttackRecord(*hitBox_))
                    {
                        AddSuppressedHitTarget(
                            hitBox_.get(),
                            guardCollider_->GetGuardOwner(),
                            suppressedHitTargets_);
                        break;
                    }
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::HitBoxCollision");
        // 最後にHitBoxとHurtBoxを判定し、接触情報付きでAttackSystemへ流す。
        BS_PROFILE_PLOT(
            "Collision.NarrowPhasePairCount",
            static_cast<int64_t>(hitBoxes_.size() * hurtBoxes_.size()));
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

                if (isOverlapping_ && !IsHitSuppressedForTarget(hitBox_.get(), hurtBox_.get(), suppressedHitTargets_) &&
                    !ShouldSuppressTriggerPair(hitBox_.get(), hurtBox_.get()))
                {
                    // 本体へ届いた接触もPlayerの防御方向で判定し、盾の小さな隙間から通常Hitへ抜けるのを防ぐ。
                    const auto hurtOwner_ = GetHierarchyRoot(hurtBox_->GetGameObject());
                    for (const auto& guard_ : guardColliders_)
                    {
                        if (!guard_->IsEnabled() || !guard_->GuardsBodyContact() ||
                            GetHierarchyRoot(guard_->GetGuardOwner()) != hurtOwner_) continue;
                        bool guarded_ = false;
                        if (guard_->IsJustGuardEnabled() && justGuardSystem)
                        {
                            JustGuardContact guardContact_{};
                            guardContact_.position = contact_.position;
                            guardContact_.normal = contact_.normal;
                            guardContact_.isValid = contact_.isValid;
                            guarded_ = justGuardSystem->HandleJustGuard(*hitBox_, *guard_, guardContact_);
                        }
                        if (!guarded_ && !guard_->HasGuardedAttackRecord(*hitBox_) && guardSystem)
                        {
                            GuardContact guardContact_{};
                            guardContact_.position = contact_.position;
                            guardContact_.normal = contact_.normal;
                            guardContact_.isValid = contact_.isValid;
                            guarded_ = guardSystem->HandleGuard(*hitBox_, *guard_, guardContact_);
                        }
                        if (guarded_ || guard_->HasGuardedAttackRecord(*hitBox_))
                        {
                            AddSuppressedHitTarget(hitBox_.get(), guard_->GetGuardOwner(), suppressedHitTargets_);
                            break;
                        }
                    }
                }
                if (isOverlapping_ &&
                    IsHitSuppressedForTarget(
                        hitBox_.get(),
                        hurtBox_.get(),
                        suppressedHitTargets_))
                {
                    isOverlapping_ = false;
                    contact_ = {};
                }

                HandleTriggerPair(
                    hitBox_.get(),
                    hurtBox_.get(),
                    isOverlapping_,
                    contact_.isValid ? &contact_ : nullptr);
            }
        }
    }

}
