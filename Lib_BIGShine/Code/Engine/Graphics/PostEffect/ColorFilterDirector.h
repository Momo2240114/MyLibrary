#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Math/EasingComponent.h"

struct ColorFilterWayPoint
{
    BloomSystem::CbColorFilter filter = {
        0.0f, 1.0f, 1.0f, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.0f };
    float duration = 1.0f;
    float holdDuration = 0.0f;
    float returnDuration = 1.0f;
    EasingType easing = EasingType::Lerp;
};

class ColorFilterDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "ColorFilterDirector";
        return name_;
    }

    void Start() override;
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    void PlayFilter(const ColorFilterWayPoint& wayPoint_);

private:
    enum class PHASE
    {
        IDLE,
        TO_TARGET,
        HOLD,
        TO_DEFAULT
    };

    void ApplyInterpolated(float ratio_);
    void BeginTransitionToDefault();
    void ResetToDefault();
    static BloomSystem::CbColorFilter LerpFilter(
        const BloomSystem::CbColorFilter& from_,
        const BloomSystem::CbColorFilter& to_,
        float ratio_);

    int easingIndex = -1;
    PHASE phase = PHASE::IDLE;
    float holdElapsed = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    EasingType activeEasing = EasingType::Lerp;
    BloomSystem::CbColorFilter startParams{};
    BloomSystem::CbColorFilter targetParams{};
    BloomSystem::CbColorFilter defaultParams = {
        0.0f, 1.0f, 1.0f, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.0f };
    std::vector<ColorFilterWayPoint> editorWaypoints;
};
