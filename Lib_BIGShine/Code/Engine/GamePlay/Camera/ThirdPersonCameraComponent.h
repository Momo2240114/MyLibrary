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
    ThirdPersonCameraComponent() = default;
    ~ThirdPersonCameraComponent() override = default;

    const std::string& GetName() const override
    {
        static std::string name = "ThirdPersonCameraComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    void ResolveTarget();
    void ResolveCamera();
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
    float followSpeed = 15.0f;

    DirectX::XMFLOAT3 targetOffset{ 0.0f, 1.5f, 0.0f };
};
