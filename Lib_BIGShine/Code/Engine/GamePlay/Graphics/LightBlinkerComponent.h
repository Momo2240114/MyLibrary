#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"

class LightComponent;

class LightBlinkerComponent : public Component
{
public:
    // 点滅モード
    // Loopは繰り返し、PingPongは往復、Onceは一度だけ強度を変化させる。
    enum class Mode { Loop, PingPong, Once };

    void Start() override;

    // EasingComponentの進行値をLightComponentのIntensity/Rangeへ反映する。
    void Update(float et) override;

    // 現在設定の速度とEasing種別で、Intensity用のEasingを開始し直す。
    void RestartIntensityEasing();

    void Gui() override;

    // セーブ用：引数の json オブジェクトに自分の変数を書き込む
    void Serialize(nlohmann::json& j) const;
        // ロード用：引数の json オブジェクトから自分の変数を読み出す
        void Deserialize(const nlohmann::json& j);

    const std::string& GetName() const override {
        static std::string name = "LightBlinker";
        return name;
    }

private:
    // 点滅対象のLightと、補間を担当するEasingComponent。
    std::shared_ptr<LightComponent> light;
    std::shared_ptr< EasingComponent> easingComp;

    bool active = true;

    // PingPong時に、現在どちらの明るさへ向かっているか。
    bool isTargetState = false;

    // 設定値
    EasingType type = EasingType::Lerp;
    float speed = 1.0f;

    // ランダム速度を使う場合の範囲。
    float minSpeed = 0.5f;
    float maxSpeed = 2.0f;
    float intensityMultiplier = 0.0f; // 0にすると消灯⇔点灯の点滅になる
    float rangeMultiplier = 0.0f;
    bool useRangeBlink = false;

    // 保存用基準値
    // 点滅前のLight設定。倍率を掛ける基準として保持する。
    float baseIntensity = 0.0f;
    float baseRange = 0.0f;

    // Easing管理用インデックス
    int intensityIdx = -1;
    int rangeIdx = -1;
};
