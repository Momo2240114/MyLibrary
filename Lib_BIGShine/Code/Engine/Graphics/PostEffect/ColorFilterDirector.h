#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"
#include "Engine/Math/EasingComponent.h"

struct ColorFilterWayPoint
{
    // 目標にするColorFilter設定。
    BloomSystem::CbColorFilter filter = {
        0.0f, 1.0f, 1.0f, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.0f };
    // 現在設定からfilterへ移る時間。
    float duration = 1.0f;
    // filter到達後、その色味を維持する時間。
    float holdDuration = 0.0f;
    // defaultParamsへ戻す時間。
    float returnDuration = 1.0f;
    // 補間に使うEasing種類。
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
    // WaypointごとのGlobalActionを登録する。
    void RegisterActions() override;
    // 指定Waypointを使ってColorFilter演出を再生する。
    void PlayFilter(const ColorFilterWayPoint& wayPoint_);

private:
    enum class PHASE
    {
        // 目標へ移動、保持、Defaultへ復帰の3段階で進む。
        IDLE,
        TO_TARGET,
        HOLD,
        TO_DEFAULT
    };

    // Easing値に応じて現在のColorFilterを反映する。
    void ApplyInterpolated(float ratio_);
    // Hold終了後、Defaultへ戻るEasingを開始する。
    void BeginTransitionToDefault();
    // GraphicsDeviceManagerのFilter設定をDefaultへ戻す。
    void ResetToDefault();
    // ColorFilter設定を線形補間する。
    static BloomSystem::CbColorFilter LerpFilter(
        const BloomSystem::CbColorFilter& from_,
        const BloomSystem::CbColorFilter& to_,
        float ratio_);

    // EasingComponent内でこのDirectorが使うEasing Slot。
    int easingIndex = -1;
    int startWaypointIndex = -1; // 指定時は初回描画より前に開始色を反映する。
    // 現在の演出Phase。
    PHASE phase = PHASE::IDLE;
    float holdElapsed = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    EasingType activeEasing = EasingType::Lerp;
    // 補間開始/目標/DefaultのFilter設定。
    BloomSystem::CbColorFilter startParams{};
    BloomSystem::CbColorFilter targetParams{};
    BloomSystem::CbColorFilter defaultParams = {
        0.0f, 1.0f, 1.0f, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.0f };
    // Editorで保存/登録するColorFilter演出一覧。
    std::vector<ColorFilterWayPoint> editorWaypoints;
};
