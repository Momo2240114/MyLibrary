#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>
#include <string>
#include <vector>

class CameraComponent;
class CameraLockOnComponent;
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

    void SetCollisionSuppressed(bool suppressed_);
    bool IsCollisionSuppressed() const { return collisionSuppressed; }

private:
    void ResolveReferences();
    bool SweepCameraSegment(
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_,
        const std::vector<const void*>& ignoredUserData_,
        float& hitDistance_) const;
    void ApplyCameraPose(
        const DirectX::XMFLOAT3& cameraPosition_,
        const DirectX::XMFLOAT3& lookAtPosition_);

private:
    std::weak_ptr<CameraComponent> camera;
    std::weak_ptr<CameraLockOnComponent> lockOnCamera;
    std::weak_ptr<GameObject> ignoreTarget;
    std::weak_ptr<GameObject> ignoreLockOnTarget;
    std::string ignoreTargetName;
    std::string ignoreLockOnTargetName;

    float collisionOffset = 0.15f;
    float minimumCameraDistance = 0.05f;
    float probeRadius = 0.3f;
    bool includeDynamicColliders = true;
    bool collisionSuppressed = false;
    bool enableLockOnCollisionLift = true;
    bool reduceCollisionLiftWhenArenaWallCameraActive = true;
    float maxCollisionLiftHeight = 3.0f;
    float collisionLiftStartRate = 0.25f;
    float collisionLiftUpSpeed = 14.0f;
    float collisionLiftDownSpeed = 5.0f;
    float currentCollisionLift = 0.0f;
    float debugDesiredDistance = 0.0f;
    float debugCorrectedDistance = 0.0f;
    float debugCompressionRate = 0.0f;
};
