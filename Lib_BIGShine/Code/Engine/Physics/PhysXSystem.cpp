#include "PhysXSystem.h"

using namespace physx;

static PxFoundation* gFoundation = nullptr;
static PxPhysics* gPhysics = nullptr;
static PxScene* gScene = nullptr;
static PxDefaultCpuDispatcher* gDispatcher = nullptr;

// PVD (PhysX Visual Debugger) 関連（未使用）
static PxPvd* gPvd = nullptr;
static PxPvdTransport* gPvdTransport = nullptr;
static PxMaterial* gDefaultMaterial = nullptr;

void PhysXSystem::Initialize()
{
    static PxDefaultErrorCallback gError;
    static PxDefaultAllocator     gAlloc;

    // Foundation
    gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAlloc, gError);

    // =============================
    // ① PVD インスタンス作成
    // =============================
    gPvd = PxCreatePvd(*gFoundation);

    // TCP transport を作成（127.0.0.1:5425）
    gPvdTransport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);

    // PVD へ接続開始
    gPvd->connect(*gPvdTransport, PxPvdInstrumentationFlag::eALL);

    // =============================
    // ② Physics を PVD 対応で作成
    // =============================
    PxTolerancesScale scale;
    gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, scale, true, gPvd);

    // =============================
    // ③ Scene 作成
    // =============================
    PxSceneDesc desc(gPhysics->getTolerancesScale());
    desc.gravity = PxVec3(0, -9.8f, 0);

    gDispatcher = PxDefaultCpuDispatcherCreate(2);
    desc.cpuDispatcher = gDispatcher;
    desc.filterShader = PxDefaultSimulationFilterShader;

    gScene = gPhysics->createScene(desc);

    //// Scene 作成後に PVD SceneClient を取得
    PxPvdSceneClient* pvdClient = gScene->getScenePvdClient();
    if (pvdClient)
    {
        // PVD に送信する情報を指定
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }

    // ★ 共通デフォルトマテリアル（摩擦・反発）
    gDefaultMaterial = gPhysics->createMaterial(0.775f, 0.5f, 0.1f);
}

void PhysXSystem::Finalize()
{
    if (gScene) { gScene->release(); gScene = nullptr; }
    if (gDispatcher) { gDispatcher->release(); gDispatcher = nullptr; }
    if (gPhysics) { gPhysics->release(); gPhysics = nullptr; }

    // PVD の後始末
    if (gPvd) {
        gPvd->release();
        gPvd = nullptr;
    }
    if (gPvdTransport) {
        gPvdTransport->release();
        gPvdTransport = nullptr;
    }

    if (gFoundation) { gFoundation->release(); gFoundation = nullptr; }
}


void PhysXSystem::Step(float dt)
{
    gScene->simulate(dt);
    gScene->fetchResults(true);
}

PxRigidDynamic* PhysXSystem::CreateBox(const PxVec3& pos, const PxVec3& halfSize)
{
    PxMaterial* mat = gPhysics->createMaterial(0.5f, 0.5f, 0.5f);
    PxRigidDynamic* box = PxCreateDynamic(
        *gPhysics,
        PxTransform(pos),
        PxBoxGeometry(halfSize),
        *mat,
        1.0f
    );
    gScene->addActor(*box);

    //OutputDebugStringA("CreateBox\n");
    return box;
}



physx::PxRigidDynamic* PhysXSystem::CreateSphere(const physx::PxVec3& pos, float radius)
{
    PxMaterial* mat = gPhysics->createMaterial(0.3f, 0.3f, 0.1f);

    PxRigidDynamic* sphere = PxCreateDynamic(
        *gPhysics,
        PxTransform(pos),
        PxSphereGeometry(radius),
        *mat,
        1.0f
    );

    gScene->addActor(*sphere);
    return sphere;
}



physx::PxRigidDynamic* PhysXSystem::CreateCapsule(const physx::PxVec3& pos, float r, float h)
{
    PxMaterial* mat = gPhysics->createMaterial(0.3f, 0.3f, 0.1f);

    PxRigidDynamic* cap = PxCreateDynamic(
        *gPhysics,
        PxTransform(pos),
        PxCapsuleGeometry(r, h),
        *mat,
        1.0f
    );

    gScene->addActor(*cap);
    return cap;
}


PxRigidStatic* PhysXSystem::CreateGround(const physx::PxVec3& pos)
{
    PxMaterial* mat = gPhysics->createMaterial(0.5f, 0.5f, 0.1f);
    PxRigidStatic* ground = gPhysics->createRigidStatic(
        PxTransform(pos)
    );

    PxShape* shape = PxRigidActorExt::createExclusiveShape(
        *ground,
        PxBoxGeometry(5, 0.2f, 5),
        *mat
    );

    gScene->addActor(*ground);
    return ground;
}

physx::PxMaterial* PhysXSystem::GetDefaultMaterial()
{
    return gDefaultMaterial;
}

PxPhysics* PhysXSystem::GetPhysics() { return gPhysics; }
PxScene* PhysXSystem::GetScene() { return gScene; }

