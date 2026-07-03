#pragma once

#include <memory>
#include <vector>


class ColliderComponent;
class JustDodgeSystem;
class AttackSystem;
struct AttackHitContact;
struct CollisionContact;

class CollisionStepSystem
{
public:
    void SetAttackSystem(AttackSystem* attackSystem_)
    {
        attackSystem = attackSystem_;
    }
    void SetJustDodgeSystem(JustDodgeSystem* justDodgeSystem_)
    {
        justDodgeSystem = justDodgeSystem_;
    }
    void RegisterCollider(const std::shared_ptr<ColliderComponent>& collider_);
    void UnregisterCollider(ColliderComponent* collider_);
    void Clear();

    void SyncFromPhysics(float deltaTime_);
    void Step(float deltaTime_);

    void HandlePhysXTrigger(
        ColliderComponent* colliderA_,
        ColliderComponent* colliderB_,
        bool hit_);
    void HandlePhysXContact(
        ColliderComponent* colliderA_,
        ColliderComponent* colliderB_,
        const CollisionContact& contactA_,
        const CollisionContact& contactB_);

private:
    void RemoveExpiredColliders();
    void StepCombatPairs();
    void HandleTriggerPair(
        ColliderComponent* colliderA_,
        ColliderComponent* colliderB_,
        bool hit_,
        const AttackHitContact* contact_);

private:
    std::vector<std::weak_ptr<ColliderComponent>> colliders;
    AttackSystem* attackSystem = nullptr;
    JustDodgeSystem* justDodgeSystem = nullptr;

};
