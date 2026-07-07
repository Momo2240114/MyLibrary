#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class CameraComponent;
class GameObject;
class ThirdPersonCameraComponent;

class CameraLockOnComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraLockOnComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetLockOnEnabled(bool enabled_);
    bool IsLockOnEnabled() const { return lockOnEnabled; }
    std::shared_ptr<GameObject> GetLockOnTarget() const { return lockOnTarget.lock(); }

private:
    void ResolveReferences();
    void RegisterActions();
    void ResetTracking();
    void ReleaseThirdPersonCamera();
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);
    DirectX::XMFLOAT3 GetFocusPosition(
        const std::shared_ptr<GameObject>& object_,
        const DirectX::XMFLOAT3& fallbackOffset_) const;
    struct LockOnDistanceInfo
    {
        DirectX::XMFLOAT3 playerPosition{};
        DirectX::XMFLOAT3 targetPosition{};
        DirectX::XMFLOAT3 difference{};
        float horizontalDistance = 0.0f;
        float targetDistance = 0.0f;
        bool isValid = false;
    };

    LockOnDistanceInfo CalculateLockOnDistanceInfo() const;
    void DrawLockOnDebugGui();
private:
    std::weak_ptr<GameObject> playerTarget;
    std::weak_ptr<GameObject> lockOnTarget;
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<ThirdPersonCameraComponent> thirdPersonCamera;

    std::string playerTargetName;
    std::string lockOnTargetName;
    bool lockOnEnabled = false;

    DirectX::XMFLOAT3 playerOffset{ 0.0f, 1.5f, 0.0f };
    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.2f, 0.0f };
    float maxDistance = 28.0f;
    float baseDistance = 7.0f;
    float distanceScale = 0.45f;
    float minCameraDistance = 6.0f;
    float maxCameraDistance = 16.0f;
    float cameraHeight = 2.2f;
    float separationHeightScale = 0.08f;
    float heightDifferenceScale = 0.35f;
    float sideOffset = 1.25f;
    float currentSide = 1.0f;
    float closeDistance = 8.0f;
    float closeFocusBias = 0.28f;
    float farFocusBias = 0.5f;
    float positionInterpolationSpeed = 10.0f;
    float lookAtInterpolationSpeed = 12.5f;

    DirectX::XMFLOAT3 interpolatedPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 interpolatedLookAt{ 0.0f, 0.0f, 0.0f };
    std::weak_ptr<GameObject> previousLockOnTarget;
    bool hasTrackingPose = false;
    bool ownsThirdPersonControl = false;
};
