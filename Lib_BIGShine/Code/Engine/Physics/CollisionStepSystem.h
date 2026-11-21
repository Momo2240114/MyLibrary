#pragma once

#include <memory>
#include <vector>


class ColliderComponent;
class HitBox;
class JustGuardSystem;
class JustDodgeSystem;
class GuardSystem;
class AttackSystem;
struct AttackHitContact;
struct CollisionContact;

// Scene内Colliderをまとめて管理し、PhysX同期と手動Combat判定を進めるSystem。
// HitBox/HurtBox/Guard/JustDodgeはGameplay用Volumeとしてここから各Systemへ渡す。
class CollisionStepSystem
{
public:
    void SetAttackSystem(AttackSystem* attackSystem_)
    {
        // HitBox/HurtBox の重なりを攻撃イベントへ変換する通知先。
        attackSystem = attackSystem_;
    }

    void SetJustDodgeSystem(JustDodgeSystem* justDodgeSystem_)
    {
        // HitBox/JustDodgeCollider の重なりをジャスト回避イベントへ変換する通知先。
        justDodgeSystem = justDodgeSystem_;
    }

    void SetJustGuardSystem(JustGuardSystem* justGuardSystem_)
    {
        // HitBox/GuardCollider の重なりをジャストガードイベントへ変換する通知先。
        justGuardSystem = justGuardSystem_;
    }

    void SetGuardSystem(GuardSystem* guardSystem_)
    {
        // HitBox/GuardCollider の重なりをガードイベントへ変換する通知先。
        guardSystem = guardSystem_;
    }

    // Scene 内に存在する Collider を弱参照で登録する。
    void RegisterCollider(const std::shared_ptr<ColliderComponent>& collider_);

    // GameObject 破棄や Component 削除時に、登録済み Collider を外す。
    void UnregisterCollider(ColliderComponent* collider_);
    void Clear();
    // 現在有効なHitBoxを収集し、Editor Debugや外部Systemから参照できるようにする。
    std::vector<std::shared_ptr<HitBox>> CollectActiveHitBoxes();

    // PhysX側のTransform/Controller状態をGame側Colliderへ取り込む。
    void SyncFromPhysics(float deltaTime_);
    void PrepareInitialPhysicsState(float deltaTime_);
    // 1Frame分のPhysics準備、手動Combat判定、Collision Stepを進める。
    void Step(float deltaTime_);

    void HandlePhysXTrigger(
        ColliderComponent* colliderA_,
        ColliderComponent* colliderB_,
        bool hit_);

    // PhysX から受け取った接触情報を Collider Callback と Gameplay 判定へ橋渡しする。
    void HandlePhysXContact(
        ColliderComponent* colliderA_,
        ColliderComponent* colliderB_,
        const CollisionContact& contactA_,
        const CollisionContact& contactB_);

private:
    // weak_ptr が切れた Collider を配列から取り除く。
    void RemoveExpiredColliders();
    // HitBox/HurtBox/Guard/JustDodgeの組み合わせを手動で判定する。
    void StepCombatPairs();
    void HandleTriggerPair(
        ColliderComponent* colliderA_,
        ColliderComponent* colliderB_,
        bool hit_,
        const AttackHitContact* contact_);

private:
    // GameObject側が所有するColliderを弱参照で保持する。
    std::vector<std::weak_ptr<ColliderComponent>> colliders;
    AttackSystem* attackSystem = nullptr;
    JustDodgeSystem* justDodgeSystem = nullptr;
    JustGuardSystem* justGuardSystem = nullptr;
    GuardSystem* guardSystem = nullptr;

};
