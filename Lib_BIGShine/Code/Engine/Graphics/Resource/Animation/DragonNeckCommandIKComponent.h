#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

class GltfModelCommponennt;
class IKMapComponent;

enum class DRAGON_NECK_IK_MODE
{
    DISABLED,
    LOOK_AT_TARGET,
    LOOK_AT_WORLD_POSITION,
    FIXED_YAW_OFFSET,
    SWEEP_YAW
};

struct DragonNeckIKCommand
{
    DRAGON_NECK_IK_MODE mode = DRAGON_NECK_IK_MODE::DISABLED;

    std::string targetName;
    DirectX::XMFLOAT3 worldTargetPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetOffset = { 0.0f, 0.0f, 0.0f };

    float startYawDegrees = 0.0f;
    float endYawDegrees = 0.0f;
    float yawOffsetDegrees = 0.0f;
    float pitchOffsetDegrees = 0.0f;

    float maxYawDegrees = 55.0f;
    float maxPitchDegrees = 30.0f;

    float duration = 1.0f;
    float blendInTime = 0.15f;
    float blendOutTime = 0.2f;

    float rootWeight = 0.2f;
    float tipWeight = 1.0f;

    bool keepLastDirection = false;
};

class DragonNeckCommandIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void RegisterActions();

    void ClearCommand();

    void LookAtTarget(
        const std::string& targetName_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    void LookAtWorldPosition(
        const DirectX::XMFLOAT3& worldPosition_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    void LookAtYawOffset(
        float yawDegrees_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    void SweepYaw(
        float startYawDegrees_,
        float endYawDegrees_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    void SetTargetObjectLookMode(bool enabled_);
    void SetTargetObjectName(const std::string& targetName_) { lookTargetObjectName = targetName_; }

    bool IsCommandActive() const { return currentCommand.mode != DRAGON_NECK_IK_MODE::DISABLED; }

    bool GetMouthWorldPosition(DirectX::XMFLOAT3& position_) const;
    bool GetMouthWorldForward(DirectX::XMFLOAT3& forward_) const;

private:
    GltfModelCommponennt* model = nullptr;
    IKMapComponent* ikMap = nullptr;

    std::string chainName = "DragonNeck";
    std::string mouthNodeName = "Mouth";
    int mouthNodeIndex = -1;

    DirectX::XMFLOAT3 localForwardAxis = { 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 actionWorldTargetPosition = { 0.0f, 2.0f, 6.0f };

    std::string actionCategory = "DragonIK";
    std::string registeredActionCategory;
    std::string registeredGlobalActionCategory;
    std::vector<std::string> registeredActionNames;
    std::vector<std::string> registeredGlobalActionNames;
    bool registerGlobalActions = false;

    float actionDuration = 1.0f;
    float actionBlendInTime = 0.15f;
    float actionBlendOutTime = 0.2f;
    float actionYawDegrees = 30.0f;
    float actionSweepStartYawDegrees = -30.0f;
    float actionSweepEndYawDegrees = 30.0f;

    bool lookTargetObjectMode = false;
    std::string lookTargetObjectName = "PlayerModel";
    DirectX::XMFLOAT3 lookTargetObjectOffset = { 0.0f, 1.2f, 0.0f };
    float lookTargetBlendInTime = 0.15f;
    float lookTargetYawOffsetDegrees = 0.0f;
    float lookTargetPitchOffsetDegrees = 0.0f;
    float lookTargetMaxYawDegrees = 65.0f;
    float lookTargetMaxPitchDegrees = 35.0f;

    DragonNeckIKCommand currentCommand;
    float commandTimer = 0.0f;

    void UnregisterActions();
    void ApplyTargetObjectLookMode();
    float CalculateBlendRate() const;
    bool ResolveTargetPosition(DirectX::XMFLOAT3& targetPosition_);
    bool ApplyNeckIK(float deltaTime_);
};
