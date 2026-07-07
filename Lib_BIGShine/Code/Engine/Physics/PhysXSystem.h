#pragma once
#include <PxPhysicsAPI.h>
#include <DirectXMath.h>
#include <vector>

class PhysXSystem
{
public:
    static void Initialize();
    static void Finalize();
    static void Step(float dt);
    static void SetTimeScale(float scale_);
    static void ComputeCharacterControllerInteractions(float deltaTime_);

    static physx::PxPhysics* GetPhysics();
    static physx::PxScene* GetScene();

    static physx::PxRigidDynamic* CreateBox(const physx::PxVec3& pos, const physx::PxVec3& halfSize);
    static physx::PxRigidDynamic* CreateSphere(const  physx::PxVec3& pos, float radius);
    static physx::PxRigidDynamic* CreateCapsule(const  physx::PxVec3& pos, float r, float h);

    static physx::PxRigidStatic* CreateGround(const physx::PxVec3& pos);

    static physx::PxMaterial* GetDefaultMaterial();
    static physx::PxQueryFilterCallback* GetCharacterControllerQueryFilterCallback();
    static physx::PxControllerBehaviorCallback* GetCharacterControllerBehaviorCallback();
    static physx::PxUserControllerHitReport* GetCharacterControllerHitReport();
    static physx::PxConvexMesh* CreateConvexMesh(const std::vector<DirectX::XMFLOAT3>& vertices_);
    static physx::PxTriangleMesh* CreateTriangleMesh(
        const std::vector<DirectX::XMFLOAT3>& vertices_,
        const std::vector<uint32_t>& indices_);

public:
    static physx::PxControllerManager* GetControllerManager();

private:
    static physx::PxControllerManager* controllerManager;
};
