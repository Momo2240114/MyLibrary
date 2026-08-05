#pragma once

#include "ColliderComponent.h"

// 円柱形状のCollider。
// 主にDebug表示や単純な円柱判定Objectに使う。
class CylinderCollider : public ColliderComponent
{
public:
    float radius = 0.5f; // 円柱の半径。
    float halfHeight = 0.5f; // 中心から上下端までの高さ。
    int segments = 16; // Debug描画時の分割数。

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
        // Collider共通設定に加えて、円柱固有の形状値を保存する。
        ColliderComponent::Serialize(j_);
        j_["Radius"] = radius;
        j_["HalfHeight"] = halfHeight;
        j_["Segments"] = segments;
    }

    void Deserialize(const nlohmann::json& j_) override
    {
        // JSONから円柱形状を復元する。
        ColliderComponent::Deserialize(j_);
        radius = j_.value("Radius", 0.5f);
        halfHeight = j_.value("HalfHeight", 0.5f);
        segments = j_.value("Segments", 16);
    }
};
