#pragma once

#include "ColliderComponent.h"

#include <string>

class GltfModelCommponennt;

class CapsuleCollider : public ColliderComponent
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CapsuleCollider";
        return name_;
    }

    float radius = 0.5f;
    float halfHeight = 1.0f;
    std::string startNodeName;
    std::string endNodeName;

    ColliderType GetColliderType() const override { return ColliderType::Capsule; }

    void Gui() override;
    void Update(float deltaTime_) override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool TryGetNodeSegment(
        DirectX::XMFLOAT3& startPosition_,
        DirectX::XMFLOAT3& endPosition_);
    Capsule GetCapsuleData();
    CollisionManifold CheckCollision(std::shared_ptr<ColliderComponent> other_) override;

private:
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
    int startNodeIndex = -1;
    int endNodeIndex = -1;
};