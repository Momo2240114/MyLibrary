#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "IKMapComponent.h"

#include <string>
#include <vector>

class AnimationControllerComponent;
class GltfModelCommponennt;

class UpperBodyLeanIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetLeanEnabled(bool enabled_) { leanEnabled = enabled_; }
    void SetMoveDirection(const DirectX::XMFLOAT3& direction_);
    void SetMoveVelocity(const DirectX::XMFLOAT3& velocity_);
    void SetLeanStrength(float strength_) { leanStrength = strength_; }
    void ResetLean();

    void SetUseAnimationFilter(bool enabled_) { useAnimationFilter = enabled_; }
    void AddApplyAnimationName(const std::string& animationName_);
    void ClearApplyAnimationNames();
    bool ShouldApplyForCurrentAnimation() const;

private:
    bool ResolveDependencies();
    bool ResolveChain(IKMapComponent::IKChain& chain_);
    DirectX::XMFLOAT3 ResolveMoveVelocity(float deltaTime_);
    DirectX::XMFLOAT3 ToOwnerLocalDirection(const DirectX::XMFLOAT3& worldDirection_) const;
    void UpdateLeanAngles(const DirectX::XMFLOAT3& localMoveDirection_, float speed_, float deltaTime_, bool shouldApply_);
    void ApplyLeanChain(IKMapComponent::IKChain& chain_);
    void DrawDebugShapes(const IKMapComponent::IKChain& chain_) const;
    std::vector<std::string> GetCurrentAnimationNames() const;
    bool MatchesAnimationName(const std::string& currentName_) const;

    std::weak_ptr<IKMapComponent> ikMap;
    std::weak_ptr<GltfModelCommponennt> model;
    std::weak_ptr<AnimationControllerComponent> animationController;

    std::string ikChainName = "UpperBodyLean";
    std::vector<float> boneWeights = { 0.15f, 0.35f, 0.35f, 0.15f };
    std::vector<std::string> applyAnimationNames = { "Walk", "Run", "Dash" };
    std::string animationNameInput;

    DirectX::XMFLOAT3 manualMoveDirection = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 manualMoveVelocity = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 previousOwnerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugLocalMoveDirection = { 0.0f, 0.0f, 0.0f };

    float maxForwardLeanAngle = 15.0f;
    float maxSideLeanAngle = 10.0f;
    float leanStrength = 1.0f;
    float smoothSpeed = 10.0f;
    float returnSpeed = 7.0f;
    float minMoveSpeed = 0.05f;
    float currentForwardLean = 0.0f;
    float currentSideLean = 0.0f;
    float debugMoveSpeed = 0.0f;

    bool leanEnabled = true;
    bool useOwnerVelocity = true;
    bool useManualDirection = false;
    bool hasManualVelocity = false;
    bool hasPreviousOwnerPosition = false;
    bool allowBackwardLean = true;
    float backwardLeanScale = 0.45f;
    bool autoNormalizeWeights = true;
    bool showDebugShapes = false;
    bool showDebugText = true;
    bool chainMissingWarning = false;

    bool useAnimationFilter = true;
    bool usePartialNameMatch = true;
    bool invertAnimationFilter = false;
    bool disableWhenNoAnimationMatched = true;
};
