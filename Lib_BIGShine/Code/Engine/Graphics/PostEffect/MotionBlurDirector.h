#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/MotionBlurSystem.h"

struct MotionBlurWayPoint
{
    MotionBlurSystem::Parameters target;
    float timeToTarget = 0.1f;
    float holdDuration = 0.0f;
    float timeToDefault = 0.1f;
};

class MotionBlurDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "MotionBlurDirector";
        return name_;
    }

    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    void Play(const MotionBlurWayPoint& wayPoint_);

private:
    enum class PHASE { IDLE, TO_TARGET, HOLD, TO_DEFAULT };

    static MotionBlurSystem::Parameters Lerp(
        const MotionBlurSystem::Parameters& from_,
        const MotionBlurSystem::Parameters& to_,
        float ratio_);
    void ResetToDefault();

    PHASE phase = PHASE::IDLE;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    MotionBlurSystem::Parameters startParams;
    MotionBlurSystem::Parameters targetParams;
    MotionBlurSystem::Parameters defaultParams;
    std::vector<MotionBlurWayPoint> editorWaypoints;
};
