#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class BlackboardComponent;
class CameraCollisionComponent;
class CameraComponent;
class CameraLockOnComponent;
class GameObject;
class ThirdPersonCameraComponent;

enum class CAMERA_ACTION_PHASE
{
    IDLE,
    ENTER,
    HOLD,
    RETURN
};

enum class CAMERA_ACTION_TARGET_MODE
{
    WORLD_POSITION,
    GAME_OBJECT,
    GAME_OBJECT_OFFSET,
    HIT_POSITION,
    PLAYER,
    LOCK_ON_TARGET
};

enum class CAMERA_ACTION_POSITION_MODE
{
    CURRENT_CAMERA_RELATIVE,
    TARGET_LOCAL_RELATIVE,
    PLAYER_FORWARD_RELATIVE,
    WORLD_POSITION
};

struct CameraActionPreset
{
    std::string actionName = "JustGuard";

    CAMERA_ACTION_TARGET_MODE targetMode =
        CAMERA_ACTION_TARGET_MODE::WORLD_POSITION;
    CAMERA_ACTION_POSITION_MODE positionMode =
        CAMERA_ACTION_POSITION_MODE::CURRENT_CAMERA_RELATIVE;

    DirectX::XMFLOAT3 cameraOffset{ 0.0f, 0.5f, -1.5f };
    DirectX::XMFLOAT3 focusOffset{ 0.0f, 0.0f, 0.0f };

    float enterDuration = 0.08f;
    float holdDuration = 0.12f;
    float returnDuration = 0.18f;

    float targetFov = 45.0f;
    float shakeStrength = 0.0f;
    float shakeFrequency = 20.0f;

    int priority = 0;

    bool suppressThirdPersonControl = true;
    bool suppressLockOnControl = true;
    bool suppressCameraCollision = false;
    bool useUnscaledTime = true;

    std::string blackboardCategory = "Guard";
    std::string focusPositionKey = "Position";
    std::string attackDirectionKey = "Normal";
};

class CameraActionDirectorComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraActionDirectorComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    void PlayActionByIndex(int index_);
    void PlayActionByName(const std::string& actionName_);
    void PlayActionAtPosition(
        const std::string& actionName_,
        const DirectX::XMFLOAT3& focusPosition_);
    void PlayActionAtPosition(
        const std::string& actionName_,
        const DirectX::XMFLOAT3& focusPosition_,
        const DirectX::XMFLOAT3& attackDirection_);
    void PlayActionForTarget(
        const std::string& actionName_,
        const std::shared_ptr<GameObject>& target_);
    void StopAction(bool immediate_);

private:
    void ResolveReferences();
    void BeginAction(
        const CameraActionPreset& preset_,
        const DirectX::XMFLOAT3& focusPosition_,
        const DirectX::XMFLOAT3& attackDirection_,
        const std::shared_ptr<GameObject>& target_);
    bool ResolvePresetFocus(
        const CameraActionPreset& preset_,
        DirectX::XMFLOAT3& focusPosition_,
        DirectX::XMFLOAT3& attackDirection_);
    bool TryReadBlackboardVector3(
        const std::string& category_,
        const std::string& key_,
        DirectX::XMFLOAT3& value_) const;
    DirectX::XMFLOAT3 ResolveTargetPosition(
        const CameraActionPreset& preset_,
        const DirectX::XMFLOAT3& fallbackPosition_) const;
    DirectX::XMFLOAT3 CalculateCameraPosition(
        const CameraActionPreset& preset_,
        const DirectX::XMFLOAT3& focusPosition_,
        const DirectX::XMFLOAT3& attackDirection_) const;
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_,
        float fov_);
    void ReleaseSuppression();
    void CaptureCurrentCameraAsPreset(CameraActionPreset& preset_) const;
    int FindPresetIndex(const std::string& actionName_) const;
    float GetPhaseDuration() const;
    float SmoothStep(float value_) const;
    float ClampFov(float fov_) const;

private:
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<ThirdPersonCameraComponent> thirdPersonCamera;
    std::weak_ptr<CameraLockOnComponent> lockOnCamera;
    std::weak_ptr<CameraCollisionComponent> collisionCamera;
    std::weak_ptr<BlackboardComponent> blackboard;
    std::weak_ptr<GameObject> activeTarget;

    std::vector<CameraActionPreset> presets;
    CameraActionPreset activePreset;
    CAMERA_ACTION_PHASE phase = CAMERA_ACTION_PHASE::IDLE;

    DirectX::XMFLOAT3 startCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 startFocusPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 actionCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 actionFocusPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 returnStartCameraPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 returnStartFocusPosition{ 0.0f, 0.0f, 0.0f };
    float startFov = 60.0f;
    float activeFov = 60.0f;
    float returnStartFov = 60.0f;
    float phaseTimer = 0.0f;
    int activePriority = -1;
    bool ownsThirdPersonSuppression = false;
    bool ownsLockOnSuppression = false;
    bool ownsCollisionSuppression = false;
};
