#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

class AnimationControllerComponent;
class GameObject;
class GltfModelCommponennt;

// IKMapComponentのLOOK_ATチェーンへ注視回転を分配するComponent
class HumanLookAtIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetLookEnabled(bool enabled_) { lookEnabled = enabled_; }
    bool IsLookEnabled() const { return lookEnabled; }
    void SetTargetName(const std::string& targetName_);

private:
    struct LookRuntimeState
    {
        float activeWeight = 0.0f;
        std::vector<DirectX::XMFLOAT3> localForwardAxes;
    };

    bool ResolveDependencies();
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    std::shared_ptr<GameObject> ResolveTarget();
    bool IsAllowedAnimationState() const;
    void ApplyLookChain(
        IKMapComponent::IKChain& chain_,
        LookRuntimeState& state_,
        const DirectX::XMFLOAT3& targetPosition_,
        float deltaTime_);
    void DrawDebugShapes(
        const IKMapComponent::IKChain& chain_,
        const DirectX::XMFLOAT3& targetPosition_) const;

private:
    IKMapComponent* ikMap = nullptr;
    GltfModelCommponennt* model = nullptr;
    AnimationControllerComponent* animationController = nullptr;
    std::weak_ptr<GameObject> target;
    std::vector<LookRuntimeState> lookStates;
    std::string targetName = "TestTarget";
    std::string idleStateName = "SS_Idle";
    DirectX::XMFLOAT3 targetOffset = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT4 debugChainColor = { 0.8f, 0.3f, 1.0f, 0.9f };
    DirectX::XMFLOAT4 debugTargetColor = { 1.0f, 0.25f, 0.15f, 0.9f };
    bool lookEnabled = true;
    bool idleOnly = true;
    bool showDebugShapes = true;
};
