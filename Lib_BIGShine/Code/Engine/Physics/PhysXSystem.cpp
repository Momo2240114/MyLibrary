#include "PhysXSystem.h"
#include "PhysXEventCallback.h"

#include <algorithm>

using namespace physx;

static PxFoundation* gFoundation = nullptr;
static PxPhysics* gPhysics = nullptr;
static PxScene* gScene = nullptr;
static PxDefaultCpuDispatcher* gDispatcher = nullptr;

// PVD (PhysX Visual Debugger) 関連（未使用）
static PxPvd* gPvd = nullptr;
static PxPvdTransport* gPvdTransport = nullptr;
static PxMaterial* gDefaultMaterial = nullptr;
static CollisionCallback gCollisionCallback;
static float gTimeScale = 1.0f;

static PxControllerManager* gControllerManager = nullptr;

namespace
{
    constexpr PxU32 ROLE_PHYSICAL = 0;
    constexpr PxU32 ROLE_CHARACTER = 1;
    constexpr PxU32 ROLE_HIT_BOX = 2;
    constexpr PxU32 ROLE_HURT_BOX = 3;
    constexpr PxU32 ROLE_SENSOR = 4;
    constexpr PxU32 ROLE_JUST_DODGE_BOX = 5;

    bool IsCctMoveIgnoredRole(PxU32 role_)
    {
        return role_ == ROLE_HIT_BOX ||
            role_ == ROLE_HURT_BOX ||
            role_ == ROLE_SENSOR ||
            role_ == ROLE_JUST_DODGE_BOX;
    }

    class CharacterControllerQueryFilterCallback : public PxQueryFilterCallback
    {
    public:
        PxQueryHitType::Enum preFilter(
            const PxFilterData&,
            const PxShape* shape_,
            const PxRigidActor*,
            PxHitFlags&) override
        {
            if (!shape_) return PxQueryHitType::eNONE;

            const PxFilterData filterData_ = shape_->getSimulationFilterData();
            if (IsCctMoveIgnoredRole(filterData_.word2))
            {
                return PxQueryHitType::eNONE;
            }

            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum postFilter(
            const PxFilterData&,
            const PxQueryHit&,
            const PxShape*,
            const PxRigidActor*) override
        {
            return PxQueryHitType::eBLOCK;
        }
    };

    class CharacterControllerBehaviorCallback : public PxControllerBehaviorCallback
    {
    public:
        PxControllerBehaviorFlags getBehaviorFlags(const PxShape& shape_, const PxActor&) override
        {
            const PxFilterData filterData_ = shape_.getSimulationFilterData();
            if (filterData_.word2 == ROLE_CHARACTER)
            {
                return PxControllerBehaviorFlag::eCCT_SLIDE;
            }
            return PxControllerBehaviorFlags();
        }

        PxControllerBehaviorFlags getBehaviorFlags(const PxController&) override
        {
            return PxControllerBehaviorFlag::eCCT_SLIDE;
        }

        PxControllerBehaviorFlags getBehaviorFlags(const PxObstacle&) override
        {
            return PxControllerBehaviorFlags();
        }
    };

    CharacterControllerQueryFilterCallback gCharacterControllerQueryFilterCallback;
    CharacterControllerBehaviorCallback gCharacterControllerBehaviorCallback;
}

static PxFilterFlags PhysXFilterShader(
    PxFilterObjectAttributes attributes0_,
    PxFilterData filterData0_,
    PxFilterObjectAttributes attributes1_,
    PxFilterData filterData1_,
    PxPairFlags& pairFlags_,
    const void*,
    PxU32)
{
    const PxU32 rolePhysical_ = 0;
    const PxU32 roleCharacter_ = 1;
    const PxU32 roleHitBox_ = 2;
    const PxU32 roleHurtBox_ = 3;
    const PxU32 ownerMask_ = 0x7FFFFFFFu;
    const PxU32 responseMask_ = 0x80000000u;

    if ((filterData0_.word3 & ownerMask_) == (filterData1_.word3 & ownerMask_))
    {
        return PxFilterFlag::eSUPPRESS;
    }

    const bool hitHurtPair_ =
        (filterData0_.word2 == roleHitBox_ && filterData1_.word2 == roleHurtBox_)
        || (filterData1_.word2 == roleHitBox_ && filterData0_.word2 == roleHurtBox_);
    const bool combatOnlyRole_ =
        filterData0_.word2 == roleHitBox_ || filterData0_.word2 == roleHurtBox_
        || filterData1_.word2 == roleHitBox_ || filterData1_.word2 == roleHurtBox_;
    if (combatOnlyRole_ && !hitHurtPair_)
    {
        return PxFilterFlag::eSUPPRESS;
    }

    if (PxFilterObjectIsTrigger(attributes0_) || PxFilterObjectIsTrigger(attributes1_))
    {
        pairFlags_ = PxPairFlag::eTRIGGER_DEFAULT | PxPairFlag::eNOTIFY_TOUCH_PERSISTS;
        return PxFilterFlag::eDEFAULT;
    }

    pairFlags_ = PxPairFlag::eCONTACT_DEFAULT
        | PxPairFlag::eNOTIFY_TOUCH_FOUND
        | PxPairFlag::eNOTIFY_TOUCH_PERSISTS
        | PxPairFlag::eNOTIFY_TOUCH_LOST
        | PxPairFlag::eNOTIFY_CONTACT_POINTS;

    const bool maskOnly_ =
        (filterData0_.word1 & filterData1_.word0) != 0
        || (filterData1_.word1 & filterData0_.word0) != 0;
    const bool bothRespond_ =
        (filterData0_.word3 & responseMask_) != 0
        && (filterData1_.word3 & responseMask_) != 0;
    const bool physicalPair_ =
        (filterData0_.word2 == rolePhysical_ || filterData0_.word2 == roleCharacter_)
        && (filterData1_.word2 == rolePhysical_ || filterData1_.word2 == roleCharacter_);
    if (maskOnly_ || !bothRespond_ || !physicalPair_)
    {
        pairFlags_.clear(PxPairFlag::eSOLVE_CONTACT);
    }
    return PxFilterFlag::eDEFAULT;
}

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
    desc.filterShader = PhysXFilterShader;
    desc.simulationEventCallback = &gCollisionCallback;
    desc.flags |= PxSceneFlag::eENABLE_CCD;

    gScene = gPhysics->createScene(desc);

    // Character Controller Manager
    gControllerManager = PxCreateControllerManager(*gScene);

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
    // Sceneより先に解放
    if (gControllerManager)
    {
        gControllerManager->release();
        gControllerManager = nullptr;
    }

    if (gScene) { gScene->release(); gScene = nullptr; }
    if (gDispatcher) { gDispatcher->release(); gDispatcher = nullptr; }
    if (gDefaultMaterial) { gDefaultMaterial->release(); gDefaultMaterial = nullptr; }
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
    const float scaledDt_ = dt * gTimeScale;
    if (!gScene || scaledDt_ <= 0.0f) return;
    gScene->simulate(scaledDt_);
    gScene->fetchResults(true);
}

void PhysXSystem::SetTimeScale(float scale_)
{
    gTimeScale = (std::max)(0.0f, (std::min)(scale_, 1.0f));
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

physx::PxQueryFilterCallback* PhysXSystem::GetCharacterControllerQueryFilterCallback()
{
    return &gCharacterControllerQueryFilterCallback;
}

physx::PxControllerBehaviorCallback* PhysXSystem::GetCharacterControllerBehaviorCallback()
{
    return &gCharacterControllerBehaviorCallback;
}

PxPhysics* PhysXSystem::GetPhysics() { return gPhysics; }
PxScene* PhysXSystem::GetScene() { return gScene; }

PxControllerManager* PhysXSystem::GetControllerManager()
{
    return gControllerManager;
}

PxConvexMesh* PhysXSystem::CreateConvexMesh(const std::vector<DirectX::XMFLOAT3>& vertices_)
{
    if (!gPhysics || vertices_.size() < 4) return nullptr;

    PxConvexMeshDesc desc_;
    desc_.points.count = static_cast<PxU32>(vertices_.size());
    desc_.points.stride = sizeof(DirectX::XMFLOAT3);
    desc_.points.data = vertices_.data();
    desc_.flags = PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eQUANTIZE_INPUT;
    desc_.vertexLimit = 255;

    PxCookingParams params_(gPhysics->getTolerancesScale());
    return PxCreateConvexMesh(params_, desc_, gPhysics->getPhysicsInsertionCallback());
}

PxTriangleMesh* PhysXSystem::CreateTriangleMesh(
    const std::vector<DirectX::XMFLOAT3>& vertices_,
    const std::vector<uint32_t>& indices_)
{
    if (!gPhysics || vertices_.size() < 3 || indices_.size() < 3) return nullptr;

    PxTriangleMeshDesc desc_;
    desc_.points.count = static_cast<PxU32>(vertices_.size());
    desc_.points.stride = sizeof(DirectX::XMFLOAT3);
    desc_.points.data = vertices_.data();
    desc_.triangles.count = static_cast<PxU32>(indices_.size() / 3);
    desc_.triangles.stride = sizeof(uint32_t) * 3;
    desc_.triangles.data = indices_.data();

    PxCookingParams params_(gPhysics->getTolerancesScale());
    return PxCreateTriangleMesh(params_, desc_, gPhysics->getPhysicsInsertionCallback());
}

