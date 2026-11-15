#pragma once
#include <PxPhysicsAPI.h>
#include <DirectXMath.h>
#include <vector>

class PhysXSystem
{
public:
    class PreviewScene
    {
    public:
        PreviewScene() = default;
        ~PreviewScene();
        PreviewScene(const PreviewScene&) = delete;
        PreviewScene& operator=(const PreviewScene&) = delete;

        bool Initialize();

        // 既存のColliderやRaycastは、このスコープ内だけプレビュー側のSceneを参照する。
        class Scope
        {
        public:
            explicit Scope(PreviewScene& preview_);
            ~Scope();
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            PreviewScene* previous;
        };

    private:
        friend class PhysXSystem;
        physx::PxScene* scene = nullptr;
        physx::PxControllerManager* controllers = nullptr;
        float timeScale = 1.0f;
    };

    // PhysXのFoundation/Physics/Scene/ControllerManagerを生成・破棄する。
    static void Initialize();
    static void Finalize();
    // 現在のTimeScaleを反映してPhysX Sceneを1Step進める。
    static void Step(float dt);
    static void SetTimeScale(float scale_);
    // PhysX CharacterController同士の押し合いや重なり回復を先に計算する。
    static void ComputeCharacterControllerInteractions(float deltaTime_);

    static physx::PxPhysics* GetPhysics();
    static physx::PxScene* GetScene();

    static physx::PxRigidDynamic* CreateBox(const physx::PxVec3& pos, const physx::PxVec3& halfSize);
    static physx::PxRigidDynamic* CreateSphere(const  physx::PxVec3& pos, float radius);
    static physx::PxRigidDynamic* CreateCapsule(const  physx::PxVec3& pos, float r, float h);

    static physx::PxRigidStatic* CreateGround(const physx::PxVec3& pos);

    static physx::PxMaterial* GetDefaultMaterial();
    // CharacterControllerのMove時に、HitBoxなどのGameplay用Triggerを無視するためのFilter。
    static physx::PxQueryFilterCallback* GetCharacterControllerQueryFilterCallback();
    static physx::PxControllerFilterCallback* GetCharacterControllerFilterCallback();
    static physx::PxControllerBehaviorCallback* GetCharacterControllerBehaviorCallback();
    // GltfModelなどから渡された頂点配列をPhysX MeshへCookする。
    static physx::PxConvexMesh* CreateConvexMesh(const std::vector<DirectX::XMFLOAT3>& vertices_);
    static physx::PxTriangleMesh* CreateTriangleMesh(
        const std::vector<DirectX::XMFLOAT3>& vertices_,
        const std::vector<uint32_t>& indices_);

public:
    static physx::PxControllerManager* GetControllerManager();

private:
    static physx::PxControllerManager* controllerManager;
};
