#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>

class HitBox;

class DebugHitHistoryResetComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "DebugHitHistoryResetComponent";
        return name_;
    }

    void Start() override;
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    void ResolveHitBox();
    void ApplySettings();
    void Rearm();

    std::weak_ptr<HitBox> hitBox;
    float damage = 5.0f;
    float hitStopDuration = 0.05f;
    float hitStopScale = 0.0f;
    float knockBackPower = 5.0f;
    float rearmInterval = 0.8f;
    float rearmTimer = 0.0f;
    bool autoActivate = true;
};
