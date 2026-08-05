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
    bool IsLoadReady() const override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 現在の開始/終了Node位置を取得する。TrailやCapsule更新の基準になる。
    bool GetSegment(
        DirectX::XMFLOAT3& startPosition_,
        DirectX::XMFLOAT3& endPosition_) const;

private:
    // targetGameObjectNameから参照するGltfModelを探す。
    std::shared_ptr<GltfModelCommponennt> ResolveTargetModel();

    // startNodeName/endNodeNameをModel内Node Indexへ解決する。
    bool ResolveNodeIndices(const std::shared_ptr<GltfModelCommponennt>& model_);

    // Transform、Particle Line、Capsuleなど関連Componentへ現在Segmentを反映する。
    void SyncTargets();

    // 追従対象モデル。
    std::weak_ptr<GltfModelCommponennt> targetModel;

    // 追従先GameObjectと、線分の開始/終了Node名。
    std::string targetGameObjectName;
    std::string startNodeName;
    std::string endNodeName;
    std::string particleEmitterName;

    // Node位置に足すローカルオフセット。
    DirectX::XMFLOAT3 startOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 endOffset = { 0.0f, 0.0f, 0.0f };
    float meshVFXThickness = 1.0f;
    float capsuleRadius = 0.1f;

    // 同期対象ごとの有効切り替え。
    bool applyTransform = true;
    bool applyParticleLine = true;
    bool applyCapsule = true;

    // Node名解決後のキャッシュ。-1なら未解決。
    int startNodeIndex = -1;
    int endNodeIndex = -1;
};
