#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"

#include <DirectXMath.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

class BlackboardComponent;
class GameObject;

enum class CAMERA_COMPOSITION_PHASE
{
    IDLE,
    ENTER,
    HOLD,
    RETURN
};

enum class CAMERA_COMPOSITION_WALL_POLICY
{
    ALLOW,
    REDUCE,
    TANGENT_ONLY,
    DISABLE
};

enum class CAMERA_COMPOSITION_SOURCE
{
    SYSTEM,
    PLAYER,
    ENEMY,
    STAGE
};

struct CameraCompositionPreset
{
    std::string presetName = "WeakAttackRight";
    CAMERA_COMPOSITION_SOURCE source = CAMERA_COMPOSITION_SOURCE::PLAYER;

    float sideOffset = 0.0f;
    float distanceOffset = 0.0f;
    float heightOffset = 0.0f;
    float lookAtHeightOffset = 0.0f;
    float focusTargetRateOffset = 0.0f;
    float fovOffset = 0.0f;

    float blendInDuration = 0.1f;
    float holdDuration = 0.0f;
    float blendOutDuration = 0.2f;

    int priority = 0;

    CAMERA_COMPOSITION_WALL_POLICY wallPolicy =
        CAMERA_COMPOSITION_WALL_POLICY::REDUCE;
    float wallSuppressionStrength = 0.8f;

    bool useUnscaledTime = false;
    bool continueUntilStop = false;
    bool useAttackDirectionSide = true;
};

class CombatCameraCompositionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CombatCameraCompositionComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    void PlayPresetByName(const std::string& presetName_);
    void PlayPresetByName(
        const std::string& presetName_,
        CAMERA_COMPOSITION_SOURCE source_);
    void StopPreset(const std::string& presetName_);
    void StopPreset(
        const std::string& presetName_,
        CAMERA_COMPOSITION_SOURCE source_);
    void ClearPreset();

private:
    struct CameraCompositionLayerState
    {
        CameraCompositionPreset activePreset;
        CAMERA_COMPOSITION_SOURCE source = CAMERA_COMPOSITION_SOURCE::PLAYER;
        CAMERA_COMPOSITION_PHASE phase = CAMERA_COMPOSITION_PHASE::IDLE;
        std::string activePresetName;
        std::string requesterName;
        float phaseTimer = 0.0f;
        int activePriority = -1;
        float lockedSideSign = 1.0f;
    };

    void ResolveReferences();
    void LoadDefaultPresets();
    void ReadBlackboardRequests();
    void BeginPreset(const CameraCompositionPreset& preset_);
    void BeginPreset(
        const CameraCompositionPreset& preset_,
        CAMERA_COMPOSITION_SOURCE source_);
    int FindPresetIndex(const std::string& presetName_) const;
    int FindPresetIndex(
        const std::string& presetName_,
        CAMERA_COMPOSITION_SOURCE source_) const;
    int GetSourceLayerIndex(CAMERA_COMPOSITION_SOURCE source_) const;
    float GetPhaseDuration(const CameraCompositionLayerState& layer_) const;
    float CalculatePhaseWeight(const CameraCompositionLayerState& layer_) const;
    float SmoothStep(float value_) const;
    float ResolveDirectionalSide(
        float sideOffset_,
        bool useAttackDirectionSide_) const;
    LockOnCameraCompositionCorrection BuildCorrection(
        const CameraCompositionLayerState& layer_) const;
    LockOnCameraCompositionCorrection BuildLayeredCorrection() const;
    LockOnCameraCompositionCorrection ClampCorrection(
        const LockOnCameraCompositionCorrection& correction_) const;
    std::string GetPlayActionName(const std::string& presetName_) const;
    const char* ToString(CAMERA_COMPOSITION_PHASE phase_) const;
    const char* ToString(CAMERA_COMPOSITION_WALL_POLICY policy_) const;
    const char* ToString(CAMERA_COMPOSITION_SOURCE source_) const;
    CAMERA_COMPOSITION_WALL_POLICY WallPolicyFromString(
        const std::string& value_) const;
    CAMERA_COMPOSITION_SOURCE SourceFromString(const std::string& value_) const;

private:
    std::weak_ptr<CameraLockOnComponent> lockOnCamera;
    std::weak_ptr<BlackboardComponent> blackboard;

    std::string targetCameraObjectName;
    std::string blackboardCategory = "CameraComposition";
    std::string playTriggerKey = "Play";
    std::string stopTriggerKey = "Stop";
    std::string clearTriggerKey = "Clear";
    std::string presetNameKey = "PresetName";
    std::string sourceKey = "Source";
    std::string requesterKey = "Requester";
    std::string attackDirectionKey = "AttackDirection";

    std::vector<CameraCompositionPreset> presets;
    std::array<CameraCompositionLayerState, 3> layers;

    float maximumSideOffset = 0.8f;
    float minimumDistanceOffset = -1.0f;
    float maximumDistanceOffset = 1.5f;
    float minimumHeightOffset = -0.5f;
    float maximumHeightOffset = 0.7f;
    float minimumFovOffset = -5.0f;
    float maximumFovOffset = 6.0f;
    float minimumFocusTargetRateOffset = -0.2f;
    float maximumFocusTargetRateOffset = 0.25f;

    LockOnCameraCompositionCorrection debugAppliedCorrection;
};
