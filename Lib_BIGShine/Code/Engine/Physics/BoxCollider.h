#pragma once

#include "ColliderComponent.h"

// BoxCollider.h (AABB前提)
class BoxCollider : public ColliderComponent {
private:
    bool usedNaviMeshSystemFlg = false;
public:

    void Start()override;

    const std::string& GetName() const override
    {
        static std::string name = "BoxCollider";
        return name;
    }

    DirectX::XMFLOAT3 halfSize{ 0.5f, 0.5f, 0.5f };

    bool GetUsedNaviMeshSystemFlg() { return usedNaviMeshSystemFlg; }


    void Gui()  override;
    void Update(float dt) override;

    // BoxCollider.h 内
    void Serialize(nlohmann::json& j) const override {
        ColliderComponent::Serialize(j);
        j["HalfSize"] = { halfSize.x, halfSize.y, halfSize.z };
        j["UsedNaviMeshSystemFlg"] = usedNaviMeshSystemFlg;
    }

    void Deserialize(const nlohmann::json& j) override {
        ColliderComponent::Deserialize(j);
        if (j.contains("HalfSize")) {
            halfSize.x = j["HalfSize"][0];
            halfSize.y = j["HalfSize"][1];
            halfSize.z = j["HalfSize"][2];
        }
        if (j.contains("UsedNaviMeshSystemFlg")) {
            usedNaviMeshSystemFlg = static_cast<bool>(j["UsedNaviMeshSystemFlg"].get<bool>());
        }

    }

    ColliderType GetColliderType() const override { return ColliderType::Box; }
    CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other) override;

    OBB CreateOBB() {
        auto t = GetGameObject()->transform;

        // 一度変数に受ける
        DirectX::XMFLOAT4X4 worldFloat = t->GetWorld();
        DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&worldFloat);

        OBB obb;
        // world.r[3] は座標 (Translation)
        obb.center = DirectX::XMVectorAdd(world.r[3],
            DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&offset), world));

        // 各軸を正規化して抽出
        obb.axes[0] = DirectX::XMVector3Normalize(world.r[0]);
        obb.axes[1] = DirectX::XMVector3Normalize(world.r[1]);
        obb.axes[2] = DirectX::XMVector3Normalize(world.r[2]);

        DirectX::XMFLOAT3 scale = t->GetWorldScale();
        obb.halfSize[0] = halfSize.x * scale.x * 2.0f;
        obb.halfSize[1] = halfSize.y * scale.y * 2.0f;
        obb.halfSize[2] = halfSize.z * scale.z * 2.0f;

        obb = obb;
        return obb;
    }
    OBB obb;
};