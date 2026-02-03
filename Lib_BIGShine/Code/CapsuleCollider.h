#pragma once

#include "ColliderComponent.h"


// CapsuleCollider.h
class CapsuleCollider : public ColliderComponent {
public:
    const std::string& GetName() const override
    {
        static std::string name = "CapsuleCollider";
        return name;
    }

    float radius = 0.5f;

    float halfHeight = 1.0f; // 中心から端までの長さ

    ColliderType GetColliderType() const override { return ColliderType::Capsule; }

    void Gui()  override;
    void Update(float dt) override;

    // CapsuleCollider.h 内
    void Serialize(nlohmann::json& j) const override {
        ColliderComponent::Serialize(j); // 親の保存を呼ぶ
        j["Radius"] = radius;
        j["HalfHeight"] = halfHeight;
    }

    void Deserialize(const nlohmann::json& j) override {
        ColliderComponent::Deserialize(j); // 親の読み込みを呼ぶ
        radius = j.value("Radius", 0.5f);
        halfHeight = j.value("HalfHeight", 1.0f);
    }

    // CapsuleCollider.h
    Capsule GetCapsuleData() {
        auto t = GetGameObject()->transform;

        // 一度変数に受ける
        DirectX::XMFLOAT4X4 worldFloat = t->GetWorld();
        DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&worldFloat);

        // Y軸方向に伸びるカプセルを想定
        DirectX::XMVECTOR up = DirectX::XMVector3Normalize(world.r[1]);
        DirectX::XMVECTOR center = world.r[3];
        float h = halfHeight * t->GetWorldScale().y;

        Capsule cap;
        cap.p0 = DirectX::XMVectorAdd(center, DirectX::XMVectorScale(up, h));
        cap.p1 = DirectX::XMVectorSubtract(center, DirectX::XMVectorScale(up, h));
        cap.radius = radius * t->GetWorldScale().x;
        return cap;
    }

    CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other) override;
};