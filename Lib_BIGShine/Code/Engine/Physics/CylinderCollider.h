#pragma once

#include "ColliderComponent.h"

class CylinderCollider : public ColliderComponent
{
public:
    float radius = 0.5f;
    float halfHeight = 0.5f;
    int segments = 16;

    const std::string& GetName() const override
    {
        static std::string name = "CylinderCollider";
        return name;
    }

    ColliderType GetColliderType() const override { return ColliderType::Cylinder; }
    void Gui() override;
    void Update(float dt_) override;

    void Serialize(nlohmann::json& j_) const override
    {
        ColliderComponent::Serialize(j_);
        j_["Radius"] = radius;
        j_["HalfHeight"] = halfHeight;
        j_["Segments"] = segments;
    }

    void Deserialize(const nlohmann::json& j_) override
    {
        ColliderComponent::Deserialize(j_);
        radius = j_.value("Radius", 0.5f);
        halfHeight = j_.value("HalfHeight", 0.5f);
        segments = j_.value("Segments", 16);
    }
};
