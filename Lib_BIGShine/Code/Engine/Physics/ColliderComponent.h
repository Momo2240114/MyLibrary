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
    enum class VelocityRotationMode{NONE,YAW_ONLY,FULL_3D};
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

    // ColliderComponent.h 内
    void Serialize(nlohmann::json& j) const override {
        j["Offset"] = { offset.x, offset.y, offset.z };
        j["IsTrigger"] = isTrigger;
        j["IsStatic"] = isStatic;
        j["UsePhysicsResponse"] = usePhysicsResponse;
        j["PhysicsControlsTransform"] = physicsControlsTransform;
        j["CollisionRole"] = static_cast<int>(collisionRole);
        j["ShapeRenderFlg"] = shapeRenderEnabled;
        j["UseGravity"] = useGravity;
        j["GravityScale"] = gravityScale;
        j["VelocityRotationMode"] = static_cast<int>(velocityRotationMode);
        j["RotateVelocityThreshold"] = rotateVelocityThreshold;

        j["Mass"] = mass;
        j["DebugPrimitiveColor"] = { debugPrimitiveColor.x,debugPrimitiveColor.y,debugPrimitiveColor.z,debugPrimitiveColor.w };
    }

    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("Offset")) {
            offset.x = j["Offset"][0];
            offset.y = j["Offset"][1];
            offset.z = j["Offset"][2];
        }
        isTrigger = j.value("IsTrigger", false);
        isStatic = j.value("IsStatic", false);
        usePhysicsResponse = j.value("UsePhysicsResponse", true);
        physicsControlsTransform = j.value("PhysicsControlsTransform", false);
        collisionRole = static_cast<CollisionRole>(j.value("CollisionRole", static_cast<int>(CollisionRole::PHYSICAL)));
        shapeRenderEnabled = j.value("ShapeRenderFlg", true);
        useGravity = j.value("UseGravity", false);
        gravityScale = j.value("GravityScale", 1.0f);
        mass = j.value("Mass", 1.0f);

        velocityRotationMode = static_cast<VelocityRotationMode>(
            j.value(
                "VelocityRotationMode",
                static_cast<int>(VelocityRotationMode::NONE)
            )
            );

        rotateVelocityThreshold =
            j.value("RotateVelocityThreshold", 0.001f);

        if (j.contains("DebugPrimitiveColor")) {
            debugPrimitiveColor.x = j["DebugPrimitiveColor"][0];
            debugPrimitiveColor.y = j["DebugPrimitiveColor"][1];
            debugPrimitiveColor.z = j["DebugPrimitiveColor"][2];
            debugPrimitiveColor.w = j["DebugPrimitiveColor"][3];
        }

    }

    void SyncFromPhysics(float dt_);
    void PreparePhysicsStep();
    void RecreatePhysXActor();
    void ReleasePhysXActor();
    CollisionRole GetCollisionRole() const { return collisionRole; }
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
    bool m_IsHitFlag = false;
    physx::PxRigidActor* actor = nullptr;
    bool actorIsStatic = false;

private:
    void CreatePhysXActor();
    void UpdatePhysXShape();
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
};



