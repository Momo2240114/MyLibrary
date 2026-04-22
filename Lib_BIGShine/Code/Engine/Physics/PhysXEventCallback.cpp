#include "PhysXEventCallback.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "PhysicsCollisionComponent.h"
#include "ColliderComponent.h"

using namespace physx;

void CollisionCallback::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
    for (PxU32 i = 0; i < count; ++i)
    {
        PxTriggerPair& p = pairs[i];

        auto* cA = reinterpret_cast<PhysicsCollisionComponent*>(p.triggerActor ? p.triggerActor->userData : nullptr);
        auto* cB = reinterpret_cast<PhysicsCollisionComponent*>(p.otherActor ? p.otherActor->userData : nullptr);

        if (!cA || !cB) continue;

        auto goA = cA->GetGameObject();
        auto goB = cB->GetGameObject();
        if (!goA || !goB) continue;

        auto colA = goA->GetComponent<ColliderComponent>();
        auto colB = goB->GetComponent<ColliderComponent>();
        if (!colA || !colB) continue;

        // PhysX 5 では status フラグではなく currentTriggers を見て判定
        bool currentlyHit = true; // PxTriggerPair が渡ってきた = 当たっている
        GameObjectManager::Instance().HandlePhysXTrigger(colA.get(), colB.get(), currentlyHit);
    }
}

void CollisionCallback::onContact(const PxContactPairHeader& header, const PxContactPair* pairs, PxU32 nbPairs)
{
    // header.actors[] は actor ポインタ（注意：削除済みの可能性もあるが簡易実装）
    PhysicsCollisionComponent* cA = reinterpret_cast<PhysicsCollisionComponent*>(header.actors[0] ? header.actors[0]->userData : nullptr);
    PhysicsCollisionComponent* cB = reinterpret_cast<PhysicsCollisionComponent*>(header.actors[1] ? header.actors[1]->userData : nullptr);

    if (!cA || !cB) return;

    auto goA = cA->GetGameObject();
    auto goB = cB->GetGameObject();
    if (!goA || !goB) return;

    auto colA = goA->GetComponent<ColliderComponent>();
    auto colB = goB->GetComponent<ColliderComponent>();
    if (!colA || !colB) return;

    // pairs[] を走査して Found（最初の接触）を検知したらコール
    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxContactPair& cp = pairs[i];
        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            GameObjectManager::Instance().HandlePhysXContact(colA.get(), colB.get());
            break;
        }
    }
}
