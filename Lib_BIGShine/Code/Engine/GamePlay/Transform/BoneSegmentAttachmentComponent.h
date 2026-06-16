#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class GltfModelCommponennt;

class BoneSegmentAttachmentComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "BoneSegmentAttachment";
        return name_;
    }

    void Update(float deltaTime_) override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool GetSegment(
        DirectX::XMFLOAT3& startPosition_,
        DirectX::XMFLOAT3& endPosition_) const;

private:
    std::shared_ptr<GltfModelCommponennt> ResolveTargetModel();
    bool ResolveNodeIndices(const std::shared_ptr<GltfModelCommponennt>& model_);
    void SyncTargets();

    std::weak_ptr<GltfModelCommponennt> targetModel;
    std::string targetGameObjectName;
    std::string startNodeName;
    std::string endNodeName;
    std::string particleEmitterName;

    DirectX::XMFLOAT3 startOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 endOffset = { 0.0f, 0.0f, 0.0f };
    float meshVFXThickness = 1.0f;
    float capsuleRadius = 0.1f;
    bool applyTransform = true;
    bool applyParticleLine = true;
    bool applyCapsule = true;

    int startNodeIndex = -1;
    int endNodeIndex = -1;
};
