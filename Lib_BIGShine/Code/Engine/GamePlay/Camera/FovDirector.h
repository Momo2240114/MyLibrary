#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

struct FovWayPoint
{
    // 演出で到達させたい縦 FOV。ズームイン/アウトの最終値になる。
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

    // WayPoint に保存された設定通りに FOV 演出を再生する。
    void PlayFov(const FovWayPoint& wayPoint_);

    // CameraDirector などから一時的に easing だけ差し替えて再生したい時に使う。
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
        // 何も再生していない状態。
        IDLE,

        // 現在 FOV から目標 FOV へ向かう往路。
        TO_TARGET,

        // 目標 FOV を一定時間維持している状態。
        HOLD,

        // デフォルト FOV へ戻る復路。
        TO_DEFAULT
    };

    // EasingComponent の ratio を実際の FOV 値へ変換して CameraComponent へ反映する。
    void ApplyInterpolated(float ratio_, float easedRatio_);

    // HOLD 終了後、現在 FOV を始点として defaultFov へ戻す補間を開始する。
    void BeginTransitionToDefault();

    // FOV を defaultFov に戻し、再生状態も IDLE にする。
    void ResetToDefault();

    // FOV 補間用の単純な線形補間。入力 ratio は内部で clamp する。
    static float Lerp(float from_, float to_, float ratio_);

private:
    // EasingComponent 内で、この FOV 演出が使うスロット番号。
    int easingIndex = -1;
    PHASE phase = PHASE::IDLE;

    // HOLD と RETURN の進行管理。HOLD は TimeScale の影響を受けない RawDelta で進める。
    float holdElapsed = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;

    // defaultFov は復帰先、start/target は現在再生中の補間端点。
    float defaultFov = 60.0f;
    float startFov = 60.0f;
    float targetFov = 60.0f;

    // 往路だけで一度大きく振るための山型補間設定。
    float peakRate = 0.0f;
    float peakValue = 0.0f;

    EasingType activeEasing = EasingType::Lerp;

    // エディタで登録した FOV 演出一覧。Action 名 Fov_0, Fov_1... と対応する。
    std::vector<FovWayPoint> editorWaypoints;
};
