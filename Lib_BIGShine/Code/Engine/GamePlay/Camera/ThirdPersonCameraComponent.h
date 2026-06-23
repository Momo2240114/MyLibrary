#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class CameraComponent;
class GameObject;

class ThirdPersonCameraComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "ThirdPersonCameraComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetControlSuppressed(bool suppressed_);
    void SynchronizeViewAngles(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);

private:
    void ResolveTarget();
    void ResolveCamera();
    void ResetFollowTracking();
    void SetTarget(std::shared_ptr<GameObject> target_);

private:
    std::weak_ptr<GameObject> target;
    std::weak_ptr<CameraComponent> camera;
    std::string targetName;

    float distance = 6.0f;
    float currentPitch = 20.0f;
    float currentYaw = 0.0f;
    float minPitch = -60.0f;
    float maxPitch = 60.0f;
    float lookSpeedX = 300.0f;
    float lookSpeedY = 200.0f;
    float mouseSensitivity = 0.1f;
    float objectFollowSpeed = 5.0f;

    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.5f, 0.0f };
    DirectX::XMFLOAT3 smoothedCameraCenter{ 0.0f, 0.0f, 0.0f };
    bool hasCameraCenter = false;
    bool controlSuppressed = false;
};
