#pragma once

#include "ColliderComponent.h"

// SphereCollider.h
class SphereCollider : public ColliderComponent {
public:
    const std::string& GetName() const override
    {
        static std::string name = "SphereCollider";
        return name;
    }


    float radius = 0.5f;

    ColliderType GetColliderType() const override
    {
        return ColliderType::Sphere;
    }

    void Gui()  override;
    void Update(float dt) override;

    // SphereCollider.h 内
    void Serialize(nlohmann::json& j) const override {
        ColliderComponent::Serialize(j);
        j["Radius"] = radius;
    }

    void Deserialize(const nlohmann::json& j) override {
        ColliderComponent::Deserialize(j);
        radius = j.value("Radius", 0.5f);
    }

    Sphere GetSphereData() {
        auto t = GetGameObject()->transform;

        // 一度変数に受ける
        DirectX::XMFLOAT3 Float3 = t->GetWorldPosition();
        DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&Float3);

        return { position, radius * t->GetWorldScale().x };
    }

    CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other) override;
};
