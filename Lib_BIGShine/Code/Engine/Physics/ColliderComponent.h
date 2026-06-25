#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <unordered_set>
#include <PxPhysicsAPI.h>

#include "Collision.h"

class ColliderComponent : public Component
{
public:
    enum class ColliderType { Box, Sphere, Capsule, Cylinder, Mesh };
    enum class CollisionRole { PHYSICAL, CHARACTER, HIT_BOX, HURT_BOX, SENSOR };
    enum class VelocityRotationMode { NONE, YAW_ONLY, FULL_3D };
    enum class CollisionContainmentMode { NONE, KEEP_INSIDE };
public:

    //現在ヒットしているコリジョン
    std::unordered_set<std::shared_ptr<GameObject>> currentTriggers;
    std::vector<CollisionManifold> m_CollisionMainFolds;

    using CollisionCallback = std::function<void(std::shared_ptr<GameObject>)>;
    std::vector<CollisionCallback> onCollisionEnterCallbacks;

    // 共通プロパティ
    DirectX::XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f }; // モデル中心からのズレ
    bool isTrigger = false;                          // すり抜けるか
    bool isStatic = false;                           // 地面や壁など動かないものか
    bool usePhysicsResponse = true;                  // 物理的な押し戻しと接触判定を有効にする
    bool physicsControlsTransform = false;           // ONにするとPhysXが位置情報をTransformに書き戻す
    bool enabled = true;                               // 有効フラグ
    bool isGrounded = false;
    bool shapeRenderEnabled = true;
    float mass = 0.0f;

    Vector4 debugPrimitiveColor = { 0,0,0,1 };
    bool useGravity = false;            // 重力フラグ
    float gravityScale = 1.0f;          // 重力の強さ倍率
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 }; // 現在の移動速度（落下速度用）
    CollisionRole collisionRole = CollisionRole::PHYSICAL;
    float shapeSizeScale = 1.0f;

    // ColliderComponent.h 内
    void Serialize(nlohmann::json& j) const override;

    void Deserialize(const nlohmann::json& j) override;

    void SyncFromPhysics(float dt_);
    void PreparePhysicsStep(float deltaTime_);
    void RecreatePhysXActor();
    void ReleasePhysXActor();
    CollisionRole GetCollisionRole() const { return collisionRole; }
    void SetVelocityRotationMode(VelocityRotationMode mode_) { velocityRotationMode = mode_; }
    VelocityRotationMode GetVelocityRotationMode() const { return velocityRotationMode; }
    bool UsesPhysicsResponse() const { return usePhysicsResponse; }
    virtual bool HasFixedCollisionRole() const { return false; }
    virtual bool HasFixedPhysicsBehavior() const { return false; }
    void ApplyRoleDefaults();

    // GUIの共通部分
    virtual void Gui();

    virtual void OnTriggerEnter(std::shared_ptr<GameObject> other) {}
    virtual void OnTriggerStay(std::shared_ptr<GameObject> other) {}
    virtual void OnTriggerExit(std::shared_ptr<GameObject> other) {}
    virtual void OnCollisionEnter(std::shared_ptr<GameObject> other) {}

    virtual CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other) { return{}; };
    virtual ColliderType GetColliderType() const = 0;

    virtual ~ColliderComponent();

    virtual void SetHitFlag(bool flag)
    {
        m_IsHitFlag = flag;
    }

    virtual bool GetHitFlag()
    {
        return m_IsHitFlag;
    }

    //追加処理
    virtual void AddCollisionMainFolds(const CollisionManifold& manifold)
    {
        m_CollisionMainFolds.push_back(manifold);
    }

    void Start()override;
protected:
    void UpdatePhysXShape();
    virtual void OnPhysicsShapePrepared() {}

    bool m_IsHitFlag = false;
    physx::PxRigidActor* actor = nullptr;
    physx::PxController* characterController = nullptr;
    bool actorIsStatic = false;

private:
    void CreateCharacterController();
    void ReleaseCharacterController();
    bool UsesCharacterController() const;
    void CreatePhysXActor();
    void ApplyDynamicActorSettings(physx::PxRigidDynamic& actor_);
    physx::PxMaterial* GetShapeMaterial();
    bool HasExternalTransformChange();
    void CaptureTransformSnapshot();
    void PushTransformToPhysics();

    physx::PxMaterial* shapeMaterial = nullptr;
    DirectX::XMFLOAT3 lastTransformPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lastTransformRotation = { 0.0f, 0.0f, 0.0f };
    bool hasTransformSnapshot = false;

    VelocityRotationMode velocityRotationMode = VelocityRotationMode::NONE;
    float rotateVelocityThreshold = 0.001f;

    bool stopCharacterSlideWhenNoInput = true;
    float characterSlideStopThreshold = 0.2f;
    bool characterGroundedForStep = false;

    CollisionContainmentMode containmentMode = CollisionContainmentMode::NONE;
};



