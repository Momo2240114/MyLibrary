#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct TimeScaleWayPoint
{
    // 到達させたいTimeScale。1.0が通常速度、0に近いほど停止に近づく。
    float targetScale = 0.4f;
    // 現在ScaleからtargetScaleへ移るまでの時間。
    float timeToTarget = 0.05f;
    // targetScaleを維持する時間。
    float holdDuration = 10.0f;
    // defaultScaleへ戻るまでの時間。
    float timeToDefault = 0.15f;
    // 演出終了後に戻す基準Scale。
    float defaultScale = 1.0f;
    // TimeScaleを適用するLayer群。Director側ではPlayerを除いた反応Layerを標準にする。
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
    // Editor登録用のGlobalActionをWaypoint数に合わせて作り直す。
    void RegisterActions() override;
    // 指定IndexのWaypointをTimeManagerへ渡して一時的なTimeScale演出を再生する。
    void Play(size_t index_);
    // このDirectorの対象Layerに残った一時Scaleを終了する。
    void ResetTimeScale();

private:
    // Editorで調整するTimeScale演出リスト。各要素が1つのGlobalActionになる。
    std::vector<TimeScaleWayPoint> editorWaypoints = { TimeScaleWayPoint{} };
};
