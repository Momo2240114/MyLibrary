#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>

class GltfModelCommponennt;
class IKMapComponent;

enum class DRAGON_TAIL_IK_MODE
{
    DISABLED,
    FOLLOW_BODY,
    AIM_TO_TARGET,
    SWEEP_LEFT,
    SWEEP_RIGHT,
    SPIN_SWEEP
};

struct DragonTailIKCommand
{
    DRAGON_TAIL_IK_MODE mode = DRAGON_TAIL_IK_MODE::FOLLOW_BODY;

    std::string targetName;

    float duration = 1.0f;
    float blendInTime = 0.1f;
    float blendOutTime = 0.2f;

    float sweepAngleDegrees = 70.0f;
    float liftAngleDegrees = 8.0f;
    float expandAngleDegrees = 15.0f;

    float rootWeight = 0.05f;
    float tipWeight = 1.0f;

    float attackStartRate = 0.25f;
    float attackEndRate = 0.75f;
};

class DragonTailCommandIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetFollowBody();
    void ClearCommand();

    void AimToTarget(
        const std::string& targetName_,
        float duration_,
        float blendInTime_ = 0.1f,
        float blendOutTime_ = 0.2f);

    void StartSweepLeft(float duration_);
    void StartSweepRight(float duration_);
    void StartSpinSweep(float duration_);

    bool IsAttackActive() const { return attackActive; }

    bool GetTailTipWorldPosition(DirectX::XMFLOAT3& position_) const;
    bool GetTailTipWorldForward(DirectX::XMFLOAT3& forward_) const;

private:
    GltfModelCommponennt* model = nullptr;
    IKMapComponent* ikMap = nullptr;

    std::string chainName = "DragonTail";
    DirectX::XMFLOAT3 localForwardAxis = { 0.0f, 0.0f, 1.0f };

    DragonTailIKCommand currentCommand;
    float commandTimer = 0.0f;
    bool attackActive = false;

    float CalculateBlendRate() const;
    float CalculateCommandRate() const;
    bool ApplyTailIK(float deltaTime_);
};