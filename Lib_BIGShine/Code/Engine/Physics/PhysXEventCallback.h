#pragma once

#include <PxPhysicsAPI.h>

class ColliderComponent;

class CollisionCallback : public physx::PxSimulationEventCallback
{
public:
    void onContact(const physx::PxContactPairHeader& header,
        const physx::PxContactPair* pairs,
        physx::PxU32 nbPairs) override;

    void onTrigger(physx::PxTriggerPair* pairs,
        physx::PxU32 count) override;

    // 使わないイベント
    void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override {}
    void onWake(physx::PxActor**, physx::PxU32) override {}
    void onSleep(physx::PxActor**, physx::PxU32) override {}
    void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, physx::PxU32) override {}
};
