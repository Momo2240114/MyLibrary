#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/VignetteSystem.h"

struct VignetteWayPoint
{
    // 目標にするVignette設定。
    VignetteSystem::Parameters target;
    // 現在設定からtargetへ補間する時間。
    float timeToTarget = 0.1f;
    // target到達後、その設定を維持する時間。
    float holdDuration = 0.0f;
    // defaultParamsへ戻す時間。
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
    // WaypointごとのGlobalActionを登録する。
    void RegisterActions() override;
    // 指定Waypointを使ってVignette演出を再生する。
    void Play(const VignetteWayPoint& wayPoint_);

private:
    // Directorの再生状態。目標へ移動、保持、Defaultへ復帰の3段階で進む。
    enum class PHASE { IDLE, TO_TARGET, HOLD, TO_DEFAULT };
    // Vignette Parametersを線形補間する。
    static VignetteSystem::Parameters Lerp(
        const VignetteSystem::Parameters& from_,
        const VignetteSystem::Parameters& to_, float ratio_);
    // GraphicsDeviceManager上のVignette設定をDefaultへ戻す。
    void ResetToDefault();

    // 現在の補間Phaseと経過時間。
    PHASE phase = PHASE::IDLE;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    VignetteSystem::Parameters startParams;
    VignetteSystem::Parameters targetParams;
    VignetteSystem::Parameters defaultParams;
    // Editorで保存/登録するVignette演出一覧。
    std::vector<VignetteWayPoint> editorWaypoints;
};
