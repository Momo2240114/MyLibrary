#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/RadialBlurSystem.h"

struct RadialBlurWayPoint
{
    RadialBlurSystem::Parameters target;
    float timeToTarget = 0.1f;
    float holdDuration = 0.0f;
    float timeToDefault = 0.1f;
};

class RadialBlurDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "RadialBlurDirector";
        return name_;
    }
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    void Play(const RadialBlurWayPoint& wayPoint_);

private:
    enum class PHASE { IDLE, TO_TARGET, HOLD, TO_DEFAULT };
    static RadialBlurSystem::Parameters Lerp(
        const RadialBlurSystem::Parameters& from_,
        const RadialBlurSystem::Parameters& to_, float ratio_);
    void ResetToDefault();

    PHASE phase = PHASE::IDLE;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    RadialBlurSystem::Parameters startParams;
    RadialBlurSystem::Parameters targetParams;
    RadialBlurSystem::Parameters defaultParams;
    std::vector<RadialBlurWayPoint> editorWaypoints;
};
