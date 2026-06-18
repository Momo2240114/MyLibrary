#pragma once

#include "TrailMeshComponent.h"

class GltfModelCommponennt;

// Tracks the root and tip of a weapon and feeds TrailMeshComponent.
// An empty target node uses the target GameObject world transform.
class WeaponTrailComponent : public TrailMeshComponent
{
public:
    WeaponTrailComponent();

    const std::string& GetName() const override
    {
        static std::string name_ = "WeaponTrailComponent";
        return name_;
    }

    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetTarget(const std::string& gameObjectName_, const std::string& nodeName_ = {});
    void SetBladeOffsets(
        const DirectX::XMFLOAT3& rootOffset_,
        const DirectX::XMFLOAT3& tipOffset_);
    void SetBladeShape(
        const DirectX::XMFLOAT3& startOffset_,
        float length_,
        const DirectX::XMFLOAT3& rotationOffsetDegrees_);
    void BeginTrail(bool clearExisting_ = true);
    void EndTrail(bool clearImmediately_ = false);
    bool GetBladeSegment(
        DirectX::XMFLOAT3& rootPosition_,
        DirectX::XMFLOAT3& tipPosition_);

private:
    std::shared_ptr<GameObject> ResolveTargetObject();
    std::shared_ptr<GltfModelCommponennt> ResolveTargetModel(
        const std::shared_ptr<GameObject>& targetObject_);
    bool ResolveTargetNodeIndex(const std::shared_ptr<GltfModelCommponennt>& model_);
    void DrawBladeGuide(
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_);
    void InvalidateTarget();

private:
    std::weak_ptr<GameObject> targetObject;
    std::weak_ptr<GltfModelCommponennt> targetModel;
    std::string targetGameObjectName;
    std::string targetNodeName;
    DirectX::XMFLOAT3 rootOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotationOffsetDegrees = { 0.0f, 0.0f, 0.0f };
    float bladeLength = 1.0f;
    float bladeGuideRadius = 0.03f;
    bool showBladeGuide = true;
    int targetNodeIndex = -1;
};
