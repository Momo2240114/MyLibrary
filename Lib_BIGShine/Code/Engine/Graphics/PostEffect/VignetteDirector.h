#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/VignetteSystem.h"

struct VignetteWayPoint
{
    VignetteSystem::Parameters target;
    float timeToTarget = 0.1f;
    float holdDuration = 0.0f;
    float timeToDefault = 0.1f;
};

class VignetteDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "VignetteDirector";
        return name_;
    }
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;
    void Play(const VignetteWayPoint& wayPoint_);

private:
    enum class PHASE { IDLE, TO_TARGET, HOLD, TO_DEFAULT };
    static VignetteSystem::Parameters Lerp(
        const VignetteSystem::Parameters& from_,
        const VignetteSystem::Parameters& to_, float ratio_);
    void ResetToDefault();

    PHASE phase = PHASE::IDLE;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    VignetteSystem::Parameters startParams;
    VignetteSystem::Parameters targetParams;
    VignetteSystem::Parameters defaultParams;
    std::vector<VignetteWayPoint> editorWaypoints;
};
