#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

#include <memory>
#include <vector>

class GltfModelCommponennt;
class HitBox;

class ShieldBlockIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    struct ShieldRuntimeState
    {
        float activeWeight = 0.0f;
    };

    bool ResolveDependencies();
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    bool FindNearestAttack(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& origin_,
        DirectX::XMFLOAT3& attackPosition_) const;
    DirectX::XMFLOAT3 ClampDirection(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& from_,
        const DirectX::XMFLOAT3& to_) const;
    bool SolveTwoBone(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& animatedPositions_,
        const DirectX::XMFLOAT3& targetDirection_,
        std::vector<DirectX::XMFLOAT3>& solvedPositions_) const;
    void ApplySolvedPose(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& solvedPositions_,
        float weight_);
    void DrawDebugShapes(
        const IKMapComponent::IKChain& chain_,
        const std::vector<DirectX::XMFLOAT3>& solvedPositions_,
        const DirectX::XMFLOAT3& attackPosition_) const;

private:
    std::weak_ptr<IKMapComponent> ikMap;
    std::weak_ptr<GltfModelCommponennt> model;
    std::vector<ShieldRuntimeState> shieldStates;
    bool enabled = true;
    bool showDebugShapes = false;
    DirectX::XMFLOAT4 debugChainColor = { 0.1f, 0.7f, 1.0f, 0.9f };
    DirectX::XMFLOAT4 debugTargetColor = { 1.0f, 0.2f, 0.1f, 0.9f };
};
