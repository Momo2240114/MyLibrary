#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/Resource/Animation/ModelAnimator.h"

#include <memory>
#include <string>
#include <vector>

class CameraLockOnComponent;

enum class MOTION_WARP_TARGET_MODE
{
    NONE,
    GAME_OBJECT,
    WORLD_POSITION,
    LOCK_ON_TARGET,
    ATTACK_ASSIST_TARGET
};

enum class MOTION_WARP_ROTATION_MODE
{
    NONE,
    FACE_TARGET,
    MATCH_TARGET_ROTATION
};

enum class MOTION_WARP_POSITION_MODE
{
    NONE,
    DIRECTION_ONLY,
    SCALE_ONLY,
    DIRECTION_AND_SCALE,
    TARGET_END_POSITION
};

enum class MOTION_WARP_PHASE
{
    IDLE,
    ACTIVE,
    RETURN
};

struct MotionWarpPreset
{
    std::string presetName = "WeakAttackWarp";
    MOTION_WARP_TARGET_MODE targetMode = MOTION_WARP_TARGET_MODE::ATTACK_ASSIST_TARGET;
    MOTION_WARP_POSITION_MODE positionMode = MOTION_WARP_POSITION_MODE::DIRECTION_AND_SCALE;
    MOTION_WARP_ROTATION_MODE rotationMode = MOTION_WARP_ROTATION_MODE::FACE_TARGET;
    float startNormalizedTime = 0.0f;
    float endNormalizedTime = 0.45f;
    float targetDistance = 1.2f;
    float positionStrength = 1.0f;
    float rotationStrength = 1.0f;
    float minimumPositionScale = 0.75f;
    float maximumPositionScale = 1.25f;
    float maximumPositionCorrectionPerFrame = 0.25f;
    float maximumTotalPositionCorrection = 1.5f;
    float maximumRotationDegrees = 45.0f;
    float maximumRotationDegreesPerSecond = 360.0f;
    float minimumRemainingRootMotionDistance = 0.01f;
    float maximumWarpTargetDistance = 12.0f;
    float minimumTargetDistance = 0.15f;
    bool warpPosition = true;
    bool warpRotation = true;
    bool horizontalOnly = true;
    bool lockTargetOnStart = true;
    bool stopWhenTargetInvalid = true;
    bool continueAcrossCombo = false;
    bool useUnscaledTime = false;
};

struct MotionWarpRequest
{
    std::string requestName;
    std::string presetName;
    std::weak_ptr<GameObject> targetObject;
    DirectX::XMFLOAT3 targetPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetForward{ 0.0f, 0.0f, 1.0f };
    float normalizedTime = 0.0f;
    float accumulatedPositionCorrection = 0.0f;
    float accumulatedRotationCorrectionDegrees = 0.0f;
    bool active = false;
};

struct MotionWarpResult
{
    ModelAnimator::RootMotionDelta rootMotionDelta;
    bool positionWarped = false;
    bool rotationWarped = false;
};

class MotionWarpComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "MotionWarpComponent";
        return name_;
    }

    ~MotionWarpComponent() override;
    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Play(const std::string& presetName_ = "");
    void Stop(const std::string& requestName_ = "");
    void Clear();
    MotionWarpResult ApplyRootMotionWarp(
        const ModelAnimator::RootMotionDelta& rootMotionDelta_,
        float normalizedTime_,
        float deltaTime_);
    bool IsWarpActive() const { return request.active; }

private:
    void EnsureDefaultPresets();
    MotionWarpPreset* FindPreset(const std::string& presetName_);
    const MotionWarpPreset* FindPreset(const std::string& presetName_) const;
    std::shared_ptr<GameObject> ResolveTarget(const MotionWarpPreset& preset_);
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();
    bool ResolveRequestTarget(MotionWarpRequest& request_, const MotionWarpPreset& preset_);
    void RegisterActions();
    void UnregisterActions();

    std::vector<MotionWarpPreset> presets;
    MotionWarpRequest request;
    std::string defaultPresetName = "WeakAttackWarp";
    std::string debugInactiveReason;
    DirectX::XMFLOAT3 debugOriginalDelta{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugWarpedDelta{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugIdealStopPosition{ 0.0f, 0.0f, 0.0f };
    float debugAppliedPositionScale = 1.0f;
    float debugAppliedYawDegrees = 0.0f;
    bool actionsRegistered = false;
};
