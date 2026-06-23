#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>
#include <string>

class CameraComponent;
class GameObject;

class CameraCollisionComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraCollisionComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    void ResolveReferences();
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);

private:
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<GameObject> ignoreTarget;
    std::weak_ptr<GameObject> ignoreLockOnTarget;
    std::string ignoreTargetName;
    std::string ignoreLockOnTargetName;

    float collisionOffset = 0.15f;
    float minimumCameraDistance = 0.05f;
    float probeRadius = 0.3f;
    bool includeDynamicColliders = true;
};
