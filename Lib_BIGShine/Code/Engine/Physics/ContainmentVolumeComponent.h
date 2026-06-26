#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

namespace physx
{
    class PxRigidStatic;
}

enum class CONTAINMENT_SHAPE
{
    CYLINDER,
    CAPSULE
};

class ContainmentVolumeComponent : public Component
{
public:
    ~ContainmentVolumeComponent() override;

    const std::string& GetName() const override
    {
        static std::string name = "ContainmentVolumeComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void ApplyContainment();
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    CONTAINMENT_SHAPE shape = CONTAINMENT_SHAPE::CYLINDER;
    uint32_t targetMask = static_cast<uint32_t>(GameObject::LayerType::Player);
    float radius = 5.0f;
    float halfHeight = 2.0f;
    float targetRadius = 0.35f;
    bool constrainHeight = true;
    int segments = 64;
    bool showDebugShape = true;
    DirectX::XMFLOAT4 debugShapeColor = { 0.0f, 0.75f, 1.0f, 1.0f };

private:
    void RebuildPhysicsVolume();
    void ReleasePhysicsVolume();
    void MarkPhysicsDirty();

    physx::PxRigidStatic* actor = nullptr;
    bool physicsDirty = true;
    CONTAINMENT_SHAPE cachedShape = CONTAINMENT_SHAPE::CYLINDER;
    float cachedRadius = -1.0f;
    float cachedHalfHeight = -1.0f;
    bool cachedConstrainHeight = false;
    int cachedSegments = -1;
    DirectX::XMFLOAT3 cachedScale = { 0.0f, 0.0f, 0.0f };
};
