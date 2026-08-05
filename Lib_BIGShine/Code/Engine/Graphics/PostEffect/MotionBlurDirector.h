#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/MotionBlurSystem.h"

struct MotionBlurWayPoint
{
    // 目標にするMotionBlur設定。
    MotionBlurSystem::Parameters target;
    // 現在設定からtargetへ補間する時間。
    float timeToTarget = 0.1f;
    // target到達後、その設定を維持する時間。
    float holdDuration = 0.0f;
    // defaultParamsへ戻す時間。
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
    // WaypointごとのGlobalActionを登録する。
    void RegisterActions() override;
    // 指定Waypointを使ってMotionBlur演出を再生する。
    void Play(const MotionBlurWayPoint& wayPoint_);

private:
    // Directorの再生状態。目標へ移動、保持、Defaultへ復帰の3段階で進む。
    enum class PHASE { IDLE, TO_TARGET, HOLD, TO_DEFAULT };

    // MotionBlur Parametersを線形補間する。
    static MotionBlurSystem::Parameters Lerp(
        const MotionBlurSystem::Parameters& from_,
        const MotionBlurSystem::Parameters& to_,
        float ratio_);
    // GraphicsDeviceManager上のMotionBlur設定をDefaultへ戻す。
    void ResetToDefault();

    // 現在の補間Phaseと経過時間。
    PHASE phase = PHASE::IDLE;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    MotionBlurSystem::Parameters startParams;
    MotionBlurSystem::Parameters targetParams;
    MotionBlurSystem::Parameters defaultParams;
    // Editorで保存/登録するMotionBlur演出一覧。
    std::vector<MotionBlurWayPoint> editorWaypoints;
};
