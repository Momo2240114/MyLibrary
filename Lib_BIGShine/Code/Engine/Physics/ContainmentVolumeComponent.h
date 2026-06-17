#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

enum class CONTAINMENT_SHAPE
{
    CYLINDER,
    CAPSULE
};

class ContainmentVolumeComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "ContainmentVolumeComponent";
        return name;
    }

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

private:
    void ConstrainObject(const std::shared_ptr<GameObject>& target_);
};
