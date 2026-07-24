#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>
#include <string>

class GameObject;

class LockOnTargetComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "LockOnTargetComponent";
        return name;
    }

    DirectX::XMFLOAT3 GetFocusPosition() const;
    std::shared_ptr<GameObject> GetCombatOwner() const;
    bool IsLockOnEnabled() const { return lockOnEnabled; }
    bool IsDefaultTarget() const { return defaultTarget; }
    float GetPriority() const { return priority; }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    DirectX::XMFLOAT3 offset{ 0.0f, 1.2f, 0.0f };
    std::string combatOwnerName;
    std::weak_ptr<GameObject> combatOwner;
    bool lockOnEnabled = true;
    bool defaultTarget = false;
    float priority = 1.0f;
};
