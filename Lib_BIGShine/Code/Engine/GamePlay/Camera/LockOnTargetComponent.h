#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class LockOnTargetComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "LockOnTargetComponent";
        return name;
    }

    DirectX::XMFLOAT3 GetFocusPosition() const;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    DirectX::XMFLOAT3 offset{ 0.0f, 1.2f, 0.0f };
};
