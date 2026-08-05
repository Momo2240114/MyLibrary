#include "PrecompiledHeader.h"

#include "PhysXSystem.h"
#include "ColliderComponent.h"
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
    // ColliderComponent::CollisionRoleをPhysX FilterDataへ詰める時のRole番号。
    // PhysXのFilterShaderやCharacterController Filterは、この番号を見て接触方法を分ける。
    constexpr PxU32 ROLE_PHYSICAL = 0;
    constexpr PxU32 ROLE_CHARACTER = 1;
    constexpr PxU32 ROLE_HIT_BOX = 2;
    constexpr PxU32 ROLE_HURT_BOX = 3;
    constexpr PxU32 ROLE_SENSOR = 4;
    constexpr PxU32 ROLE_JUST_DODGE_BOX = 5;
    constexpr PxU32 ROLE_GUARD_BOX = 6;
    constexpr PxU32 COLLISION_OWNER_MASK = 0x7FFFFFFFu;

    bool HasSameCollisionOwner(PxU32 ownerIdA_, PxU32 ownerIdB_)
    {
        // 同じObject階層に属するCollider同士は、自分自身との接触として扱い抑制する。
        return ownerIdA_ != 0u && ownerIdB_ != 0u && ownerIdA_ == ownerIdB_;
    }

    bool IsCctMoveIgnoredRole(PxU32 role_)
    {
        // CharacterControllerの移動Queryでは、Combat用Volumeを地形や壁として扱わない。
        // HitBox/HurtBox/Guard/JustDodgeはCollisionStepSystem側で手動判定する。
        return role_ == ROLE_HIT_BOX ||
            role_ == ROLE_HURT_BOX ||
            role_ == ROLE_SENSOR ||
            role_ == ROLE_JUST_DODGE_BOX ||
            role_ == ROLE_GUARD_BOX;
    }

    class CharacterControllerQueryFilterCallback : public PxQueryFilterCallback
    {
    public:
        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData_,
            const PxShape* shape_,
            const PxRigidActor*,
            PxHitFlags&) override
        {
            // CCT Move中に当たるShapeを選別する。
            // Combat用Volumeと同一OwnerのShapeは無視し、それ以外は移動を止めるBLOCKにする。
            if (!shape_) return PxQueryHitType::eNONE;

            const PxFilterData shapeFilterData_ = shape_->getSimulationFilterData();
            if (IsCctMoveIgnoredRole(shapeFilterData_.word2))
            {
                return PxQueryHitType::eNONE;
            }

            const PxU32 queryOwnerId_ = filterData_.word3 & COLLISION_OWNER_MASK;
            const PxU32 shapeOwnerId_ = shapeFilterData_.word3 & COLLISION_OWNER_MASK;
            if (HasSameCollisionOwner(queryOwnerId_, shapeOwnerId_))
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
            // 地面として扱えないColliderや他Characterに乗った時は、上に立たず滑る挙動にする。
            const PxRigidActor* actor_ = shape_.getActor();
            const ColliderComponent* collider_ = actor_
                ? static_cast<const ColliderComponent*>(actor_->userData)
                : nullptr;
            if (collider_ && !collider_->CanBeCharacterGround())
            {
                return PxControllerBehaviorFlag::eCCT_SLIDE;
            }

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

    class CharacterControllerFilterCallback : public PxControllerFilterCallback
    {
    public:
        bool filter(const PxController& controllerA_, const PxController& controllerB_) override
        {
            // CharacterController同士の相互作用で、同一Owner内のControllerは無視する。
            const ColliderComponent* colliderA_ =
                static_cast<const ColliderComponent*>(controllerA_.getUserData());
            const ColliderComponent* colliderB_ =
                static_cast<const ColliderComponent*>(controllerB_.getUserData());
            if (!colliderA_ || !colliderB_) return true;

            if (HasSameCollisionOwner(
                const_cast<ColliderComponent*>(colliderA_)->GetCollisionOwnerId(),
                const_cast<ColliderComponent*>(colliderB_)->GetCollisionOwnerId()))
            {
                return false;
            }

            return true;
        }
    };

    CharacterControllerQueryFilterCallback gCharacterControllerQueryFilterCallback;
    CharacterControllerFilterCallback gCharacterControllerFilterCallback;
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
    // PhysX Scene全体の接触ルール。
    // 物理反応するColliderだけContact Solveし、Combat用Volumeは手動判定へ回すためPhysXでは抑制する。
    const PxU32 rolePhysical_ = 0;
    const PxU32 roleCharacter_ = 1;
    const PxU32 roleHitBox_ = 2;
    const PxU32 roleHurtBox_ = 3;
    const PxU32 roleJustDodgeBox_ = 5;
    const PxU32 roleGuardBox_ = 6;
    const PxU32 ownerMask_ = 0x7FFFFFFFu;
    const PxU32 responseMask_ = 0x80000000u;

    const PxU32 ownerId0_ = filterData0_.word3 & ownerMask_;
    const PxU32 ownerId1_ = filterData1_.word3 & ownerMask_;
    if (HasSameCollisionOwner(ownerId0_, ownerId1_))
    {
        // 同じ所有者のCollider同士は自分自身の部位判定なので、PhysX接触を発生させない。
        return PxFilterFlag::eSUPPRESS;
    }

    const bool combatOnlyRole_ =
        filterData0_.word2 == roleHitBox_ || filterData0_.word2 == roleHurtBox_
        || filterData0_.word2 == roleJustDodgeBox_
        || filterData0_.word2 == roleGuardBox_
        || filterData1_.word2 == roleHitBox_ || filterData1_.word2 == roleHurtBox_
        || filterData1_.word2 == roleJustDodgeBox_
        || filterData1_.word2 == roleGuardBox_;
    if (combatOnlyRole_)
    {
        // HitBox/HurtBox/Guard/JustDodgeはCollisionStepSystemがCapsule同士で判定する。
        // PhysX Triggerにも流すと二重通知になるため、ここでは完全に抑制する。
        return PxFilterFlag::eSUPPRESS;
    }

    if (PxFilterObjectIsTrigger(attributes0_) || PxFilterObjectIsTrigger(attributes1_))
    {
        // 通常TriggerはEnter/Stayを受け取れるようにする。
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
    const bool characterPair_ =
        filterData0_.word2 == roleCharacter_ &&
        filterData1_.word2 == roleCharacter_;
    const bool physicalPair_ =
        (filterData0_.word2 == rolePhysical_ || filterData0_.word2 == roleCharacter_)
        && (filterData1_.word2 == rolePhysical_ || filterData1_.word2 == roleCharacter_);
    if (maskOnly_ || !bothRespond_ || !physicalPair_ || characterPair_)
    {
        // Layer Maskだけで「検知はするが押し返さない」組み合わせや、
        // Character同士の押し合いはContact Solveを外す。
        pairFlags_.clear(PxPairFlag::eSOLVE_CONTACT);
    }
    return PxFilterFlag::eDEFAULT;
}

void PhysXSystem::Initialize()
{
    // PhysXの基本Object、Scene、PVD、CharacterControllerManagerをまとめて初期化する。
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
    if (gControllerManager)
    {
        gControllerManager->setOverlapRecoveryModule(true);
    }

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
    // Sceneより先にControllerManagerを解放し、PhysX内部参照が残らない順番で後始末する。
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
    // Game側TimeScaleを反映したdeltaでPhysX Sceneを進める。
    // scaleが0のHitStop中はsimulateせず、物理状態を止める。
    const float scaledDt_ = dt * gTimeScale;
    if (!gScene || scaledDt_ <= 0.0f) return;
    gScene->simulate(scaledDt_);
    gScene->fetchResults(true);
}

void PhysXSystem::SetTimeScale(float scale_)
{
    // PhysX側では0から1の範囲だけを受け付け、逆再生や過剰加速はここで防ぐ。
    gTimeScale = (std::max)(0.0f, (std::min)(scale_, 1.0f));
}

void PhysXSystem::ComputeCharacterControllerInteractions(float deltaTime_)
{
    // CharacterController同士の重なり回復を、ColliderComponent更新前に計算する。
    if (!gControllerManager || deltaTime_ <= 0.0f) return;
    gControllerManager->computeInteractions(
        deltaTime_,
        &gCharacterControllerFilterCallback);
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

physx::PxControllerFilterCallback* PhysXSystem::GetCharacterControllerFilterCallback()
{
    return &gCharacterControllerFilterCallback;
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
    // Dynamicにも使えるConvex MeshをCookする。PhysXの制限に合わせて頂点数を255へ制限する。
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
    // Static地形や背景用のTriangle MeshをCookする。Dynamic Actorには使わない前提。
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
