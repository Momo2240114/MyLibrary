#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

class GltfModelCommponennt;

// IKMapComponentのFOOTチェーンを地面へ追従させる多関節IKコンポーネント。
class FootIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    struct FootRuntimeState
    {
        DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 groundNormal = { 0.0f, 1.0f, 0.0f };
        bool initialized = false;
        bool grounded = false;
    };

    bool ResolveDependencies();
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    bool QueryGround(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& footPosition_,
        DirectX::XMFLOAT3& hitPosition_,
        DirectX::XMFLOAT3& hitNormal_) const;
    void SolveChain(
        IKMapComponent::IKChain& chain_,
        FootRuntimeState& state_,
        float deltaTime_);
    void ApplySolvedPose(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& solvedPositions_);
    void AlignFootToGround(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& groundNormal_,
        float deltaTime_);
    void ApplyPoleConstraint(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& animatedPositions_,
        std::vector<DirectX::XMFLOAT3>& solvedPositions_) const;
    void DrawDebugShapes(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& animatedFootPosition_,
        const FootRuntimeState& state_,
        const std::vector<DirectX::XMFLOAT3>& solvedPositions_) const;
    void ApplyPelvisAdjustment(
        const std::vector<IKMapComponent::IKChain*>& footChains_,
        float deltaTime_);
    int FindCommonAncestor(
        const std::vector<IKMapComponent::IKChain*>& footChains_) const;
    void ApplyPelvisOffset(int pelvisNodeIndex_, float worldDrop_);

    std::weak_ptr<IKMapComponent> ikMap;
    std::weak_ptr<GltfModelCommponennt> model;
    std::vector<FootRuntimeState> footStates;
    int adjustedPelvisNodeIndex = -1;
    DirectX::XMFLOAT3 pelvisBasePosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 pelvisAppliedPosition = { 0.0f, 0.0f, 0.0f };
    float currentPelvisDrop = 0.0f;
    bool pelvisPositionApplied = false;
    bool adjustPelvisHeight = true;
    float maxPelvisDrop = 0.5f;
    float pelvisPositionSpeed = 12.0f;
    float footReachTolerance = 0.01f;
    bool showDebugShapes = true;
    float debugNormalLength = 0.2f;
    DirectX::XMFLOAT4 debugChainColor = { 0.2f, 0.8f, 1.0f, 0.9f };
    DirectX::XMFLOAT4 debugRayColor = { 1.0f, 0.8f, 0.1f, 0.8f };
    DirectX::XMFLOAT4 debugGroundColor = { 0.2f, 1.0f, 0.3f, 0.9f };
    bool enabled = true;
};
