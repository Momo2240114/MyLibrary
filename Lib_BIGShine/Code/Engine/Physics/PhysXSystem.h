#pragma once
#include <PxPhysicsAPI.h>

class PhysXSystem
{
public:
    static void Initialize();
    static void Finalize();
    static void Step(float dt);

    static physx::PxPhysics* GetPhysics();
    static physx::PxScene* GetScene();

    static physx::PxRigidDynamic* CreateBox(const physx::PxVec3& pos, const physx::PxVec3& halfSize);
    static physx::PxRigidDynamic* CreateSphere(const  physx::PxVec3& pos, float radius);
    static physx::PxRigidDynamic* CreateCapsule(const  physx::PxVec3& pos, float r, float h);

    static physx::PxRigidStatic* CreateGround(const physx::PxVec3& pos);

    static physx::PxMaterial* GetDefaultMaterial();
};
