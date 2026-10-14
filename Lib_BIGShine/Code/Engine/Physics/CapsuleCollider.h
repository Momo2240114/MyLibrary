#pragma once

#include "ColliderComponent.h"

#include <string>

class GltfModelCommponennt;

// Capsule形状のCollider。
// 通常の位置固定Capsuleに加え、Model Node間へ追従するBone/部位用Capsuleとしても使える。
class CapsuleCollider : public ColliderComponent
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CapsuleCollider";
        return name_;
    }

    // Node始点を維持したまま武器判定の長さを調整する。既存Colliderは等倍。
    float nodeSegmentScale = 1.0f;
    float radius = 0.5f; // Capsule半径。
    float halfHeight = 1.0f; // 中心から上下球中心までの距離。
    std::string nodeModelObjectName; // Node追従時に参照するModel Object名。
    std::string startNodeName; // Capsule始点に使うNode名。
    std::string endNodeName; // Capsule終点に使うNode名。

    ColliderType GetColliderType() const override { return ColliderType::Capsule; }

    void Gui() override;
    void Update(float deltaTime_) override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool TryGetNodeSegment(
        DirectX::XMFLOAT3& startPosition_,
        DirectX::XMFLOAT3& endPosition_);
    // 現在のTransform/Node追従状態からCollisionMath用Capsuleを作る。
    Capsule GetCapsuleData();
    CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other_) override;

private:
    // PhysX Shape作成前にNode追従形状を反映する。
    void OnPhysicsShapePrepared() override;
    std::shared_ptr<GltfModelCommponennt> ResolveNodeModel();
    bool ResolveNodeIndex(
        const std::shared_ptr<GltfModelCommponennt>& model_,
        const std::string& nodeName_,
        int& nodeIndex_);
    void UpdateNodeSegmentShape(
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_);

private:
    // Node名解決後のIndex Cache。
    int startNodeIndex = -1;
    int endNodeIndex = -1;
};
