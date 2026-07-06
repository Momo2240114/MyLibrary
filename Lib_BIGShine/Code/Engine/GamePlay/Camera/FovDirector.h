#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct FovWayPoint
{
    float fov = 60.0f;

    // 目標FOVへ到達する時間
    float duration = 1.0f;

    // 目標FOVを維持する時間
    float holdDuration = 0.0f;

    // DefaultFOVへ戻る時間
    float returnDuration = 1.0f;

    // 往路で一度だけPeak値を経由したい場合に使う
    // 0以下なら通常のEasing補間
    float peakRate = 0.0f;
    float peakValue = 0.0f;

    EasingType easing = EasingType::Lerp;
};


class FovDirector : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "FovDirector";
        return name_;
    }

    void Start() override;
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    void PlayFov(const FovWayPoint& wayPoint_);
    void PlayFov(const FovWayPoint& wayPoint_, EasingType type_);

    // --- 親(CameraDirector)から呼ばれるヘルパー関数 ---
    int GetCount() const
    {
        return static_cast<int>(editorWaypoints.size());
    }

    void CaptureCurrentState();
    void PlayFovByIndex(int index_, EasingType type_);

private:
    enum class PHASE
    {
        IDLE,
        TO_TARGET,
        HOLD,
        TO_DEFAULT
    };

    void ApplyInterpolated(float ratio_, float easedRatio_);
    void BeginTransitionToDefault();
    void ResetToDefault();

    static float Lerp(float from_, float to_, float ratio_);

private:
    int easingIndex = -1;
    PHASE phase = PHASE::IDLE;

    float holdElapsed = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;

    float defaultFov = 60.0f;
    float startFov = 60.0f;
    float targetFov = 60.0f;

    float peakRate = 0.0f;
    float peakValue = 0.0f;

    EasingType activeEasing = EasingType::Lerp;

    std::vector<FovWayPoint> editorWaypoints;
};