#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <cstdint>
#include <unordered_set>
#include <PxPhysicsAPI.h>

#include "Collision.h"

class ColliderComponent : public Component
{
public:
    // 形状ごとの手動衝突判定や PhysX Shape 作成で分岐する種類。
    enum class ColliderType { Box, Sphere, Capsule, Cylinder, Mesh };

    // Collider が Gameplay 上どの役割かを表す。HitBox/HurtBox などは専用 System へ渡される。
    enum class CollisionRole { PHYSICAL, CHARACTER, HIT_BOX, HURT_BOX, SENSOR, JUST_DODGE_BOX, GUARD_BOX, };

    // velocity の向きへ Transform を回す時の制御方式。
    enum class VelocityRotationMode { NONE, YAW_ONLY, FULL_3D };

    // ContainmentVolume など、範囲内へ押し戻す追加制約の種類。
    enum class CollisionContainmentMode { NONE, KEEP_INSIDE };

public:

    //現在ヒットしているコリジョン
    // Trigger Enter/Exit の差分を取るため、前フレームからの接触相手を保持する。
    std::unordered_set<std::shared_ptr<GameObject>> currentTriggers;

    // 物理ステップ中に集めた接触面情報。押し戻しや接地判定で参照する。
    std::vector<CollisionManifold> m_CollisionMainFolds;

    using CollisionCallback = std::function<void(std::shared_ptr<GameObject>)>;
    using CollisionContactCallback = std::function<void(
        std::shared_ptr<GameObject>,
        const CollisionContact&)>;
    std::vector<CollisionCallback> onCollisionEnterCallbacks;
    std::vector<CollisionCallback> onTriggerEnterCallbacks;

    // 接触点・法線込みで通知したい場合の Callback。
    std::vector<CollisionContactCallback> onCollisionContactCallbacks;

    // 共通プロパティ
    DirectX::XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f }; // モデル中心からのズレ
    bool isTrigger = false;                          // すり抜けるか
    bool isStatic = false;                           // 地面や壁など動かないものか
    bool usePhysicsResponse = true;                  // 物理的な押し戻しと接触判定を有効にする
    bool physicsControlsTransform = false;           // ONにするとPhysXが位置情報をTransformに書き戻す
    bool enabled = true;                               // 有効フラグ

    // キャラクター移動やジャンプ判定で使う、現在接地しているかの結果。
    bool isGrounded = false;

    // デバッグ描画で Shape を表示するかどうか。
    bool shapeRenderEnabled = true;
    bool canBeCharacterGround = true;
    float mass = 0.0f;

    Vector4 debugPrimitiveColor = { 0,0,0,1 };
    bool useGravity = false;            // 重力フラグ
    float gravityScale = 1.0f;          // 重力の強さ倍率
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 }; // 現在の移動速度（落下速度用）
    CollisionRole collisionRole = CollisionRole::PHYSICAL;
    float shapeSizeScale = 1.0f;
    std::string colliderName;
    std::string collisionOwnerObjectName;

    // ColliderComponent.h 内
    void Serialize(nlohmann::json& j) const override;

    void Deserialize(const nlohmann::json& j) override;

    // PhysX 側で動いた Actor/Controller の位置を GameObject Transform へ同期する。
    void SyncFromPhysics(float dt_);

    // GameObject Transform や速度を PhysX Step 前の Actor/Controller へ反映する。
    void PreparePhysicsStep(float deltaTime_);

    // 設定変更後に PhysX Actor/Shape を作り直す。
    void RecreatePhysXActor();
    void ReleasePhysXActor();

    CollisionRole GetCollisionRole() const
    {
        return collisionRole;
    }

    // 直前のCCT移動が側面で制限されたかを、地上移動の実速度補正へ公開する。
    bool WasHorizontalMovementBlocked() const
    {
        return horizontalMovementBlocked;
    }

    // collisionOwnerObjectName が指定されていれば、その GameObject を判定上の所有者として解決する。
    std::shared_ptr<GameObject> ResolveCollisionOwnerObject();
    uint32_t GetCollisionOwnerId();

    const std::string& GetColliderName() const
    {
        return colliderName;
    }

    const std::string& GetCollisionOwnerObjectName() const
    {
        return collisionOwnerObjectName;
    }

    float GetMass() const
    {
        return mass;
    }

    void SetVelocityRotationMode(VelocityRotationMode mode_)
    {
        velocityRotationMode = mode_;
    }

    VelocityRotationMode GetVelocityRotationMode() const
    {
        return velocityRotationMode;
    }

    bool UsesPhysicsResponse() const
    {
        return usePhysicsResponse;
    }

    bool CanBeCharacterGround() const
    {
        return canBeCharacterGround;
    }

    // 派生 Collider が Role を固定している場合、GUI/Deserialize の変更から保護する。
    virtual bool HasFixedCollisionRole() const
    {
        return false;
    }

    // HitBox など物理挙動を固定したい Collider が上書きする。
    virtual bool HasFixedPhysicsBehavior() const
    {
        return false;
    }

    // CollisionRole に応じた isTrigger/usePhysicsResponse などの既定値を適用する。
    void ApplyRoleDefaults();
    // GUIの共通部分
    virtual void Gui();

    virtual void OnTriggerEnter(std::shared_ptr<GameObject> other)
    {
    }

    virtual void OnTriggerStay(std::shared_ptr<GameObject> other)
    {
    }

    virtual void OnTriggerExit(std::shared_ptr<GameObject> other)
    {
    }

    virtual void OnCollisionEnter(std::shared_ptr<GameObject> other)
    {
    }

    virtual CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other)
    {
        return{};
    };

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
    // Collider 固有の形状情報を PhysX Shape へ反映する。
    void UpdatePhysXShape();

    // 派生クラスが Shape 作成直前に半径や高さを調整したい時のフック。
    virtual void OnPhysicsShapePrepared()
    {
    }

    bool m_IsHitFlag = false;

    // 通常 Collider 用の PhysX Actor と、CharacterCollider 用 Controller。
    physx::PxRigidActor* actor = nullptr;
    physx::PxController* characterController = nullptr;
    bool actorIsStatic = false;
    bool horizontalMovementBlocked = false; // 物理ステップ間だけ保持するCCTの結果。

private:
    // Character role の場合に PxController を作成する。
    void CreateCharacterController();
    void ReleaseCharacterController();
    bool UsesCharacterController() const;

    // CollisionRole と isStatic に応じて PxRigidStatic/PxRigidDynamic を作る。
    void CreatePhysXActor();

    // 質量、重力、回転固定など Dynamic Actor 固有の設定を適用する。
    void ApplyDynamicActorSettings(physx::PxRigidDynamic& actor_);
    physx::PxMaterial* GetShapeMaterial();

    // 外部から Transform が直接変更されたかを見て、PhysX へ押し戻す必要を判断する。
    bool HasExternalTransformChange();
    void CaptureTransformSnapshot();
    void PushTransformToPhysics();

    // Shape 生成時に使う PhysX Material。Collider ごとに保持して寿命を管理する。
    physx::PxMaterial* shapeMaterial = nullptr;
    DirectX::XMFLOAT3 lastTransformPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lastTransformRotation = { 0.0f, 0.0f, 0.0f };
    bool hasTransformSnapshot = false;

    // velocity に合わせて向きを変えるための設定。
    VelocityRotationMode velocityRotationMode = VelocityRotationMode::NONE;
    float rotateVelocityThreshold = 0.001f;

    // CharacterController の入力停止時に、壁沿い滑りを抑えるための設定。
    bool stopCharacterSlideWhenNoInput = true;
    float characterSlideStopThreshold = 0.2f;
    bool characterGroundedForStep = false;
    CollisionContainmentMode containmentMode = CollisionContainmentMode::NONE;
};
