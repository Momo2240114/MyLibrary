#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ColliderComponent.h"
#include <PxPhysicsAPI.h>
#include <DirectXMath.h>
#include <unordered_set>

class PhysicsCollisionComponent : public Component
{
public:
    enum class ShapeType { None, Box, Sphere, Capsule };

    PhysicsCollisionComponent() = default;
    ~PhysicsCollisionComponent();

    const std::string& GetName() const override
    {
        static std::string name = "PhysicsCollisionComponent";
        return name;
    }

    void SetBox(const DirectX::XMFLOAT3& half);
    void SetSphere(float r);
    void SetCapsule(float r, float h);

    void SetKinematic(bool k) { isKinematic = k; }
    void SetUseGravity(bool g) { useGravity = g; }

    void OnCollisionEnter(std::shared_ptr<GameObject> other);
    void OnTriggerEnter(std::shared_ptr<GameObject> other);
    void OnTriggerStay(std::shared_ptr<GameObject> other);
    void OnTriggerExit(std::shared_ptr<GameObject> other);

    void Start() override { CreateActor(); }
    void Update(float dt) override;

    void Gui();

    // setter
    void SetCollisionOffset(const DirectX::XMFLOAT3& off) { collisionOffset = off; }
    void SetLockRotation(bool x, bool y, bool z) {
        lockRotationX = x; lockRotationY = y; lockRotationZ = z;
        if (actor) ApplyLockRotation(); // すでに生成済みなら適用
    }

private:
    void ApplyLockRotation(); // 実際にPhysXに適用する関数
    void CreateActor();

private:
    ShapeType shapeType = ShapeType::None;

    // コリジョンの中心位置をずらすためのオフセット
    DirectX::XMFLOAT3 collisionOffset{ 0.0f, 0.0f, 0.0f };

    // 物理による回転を固定するフラグ
    bool lockRotationX = false;
    bool lockRotationY = false;
    bool lockRotationZ = false;

    DirectX::XMFLOAT3 boxHalfSize{ 0.5f,0.5f,0.5f };
    float sphereRadius = 0.5f;
    float capsuleRadius = 0.5f;
    float capsuleHalfHeight = 1.0f;

    // Trigger 用フラグ
    bool isTrigger = false;

    bool isKinematic = false;
    bool useGravity = true;
    bool pendingCreate = false;

    physx::PxRigidDynamic* actor = nullptr;
};
