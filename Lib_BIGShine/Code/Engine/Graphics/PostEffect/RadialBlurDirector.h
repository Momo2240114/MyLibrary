#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/RadialBlurSystem.h"

struct RadialBlurWayPoint
{
    // 目標にするRadialBlur設定。
    RadialBlurSystem::Parameters target;
    // 現在設定からtargetへ補間する時間。
    float timeToTarget = 0.1f;
    // target到達後、その設定を維持する時間。
    float holdDuration = 0.0f;
    // defaultParamsへ戻す時間。
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
    // WaypointごとのGlobalActionを登録する。
    void RegisterActions() override;
    // 指定Waypointを使ってRadialBlur演出を再生する。
    void Play(const RadialBlurWayPoint& wayPoint_);

private:
    // Directorの再生状態。目標へ移動、保持、Defaultへ復帰の3段階で進む。
    enum class PHASE { IDLE, TO_TARGET, HOLD, TO_DEFAULT };
    // RadialBlur Parametersを線形補間する。
    static RadialBlurSystem::Parameters Lerp(
        const RadialBlurSystem::Parameters& from_,
        const RadialBlurSystem::Parameters& to_, float ratio_);
    // GraphicsDeviceManager上のRadialBlur設定をDefaultへ戻す。
    void ResetToDefault();

    // 現在の補間Phaseと経過時間。
    PHASE phase = PHASE::IDLE;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    RadialBlurSystem::Parameters startParams;
    RadialBlurSystem::Parameters targetParams;
    RadialBlurSystem::Parameters defaultParams;
    // Editorで保存/登録するRadialBlur演出一覧。
    std::vector<RadialBlurWayPoint> editorWaypoints;
};
