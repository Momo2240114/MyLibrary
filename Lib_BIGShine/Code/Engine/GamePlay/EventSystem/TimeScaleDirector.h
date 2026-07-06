#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct TimeScaleWayPoint
{
    float targetScale = 0.4f;
    float timeToTarget = 0.05f;
    float holdDuration = 10.0f;
    float timeToDefault = 0.15f;
    float defaultScale = 1.0f;
    uint32_t layerMask = 0xffffffffu;
};

class TimeScaleDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "TimeScaleDirector";
        return name_;
    }

    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    void Play(size_t index_);

private:
    std::vector<TimeScaleWayPoint> editorWaypoints = { TimeScaleWayPoint{} };
};
