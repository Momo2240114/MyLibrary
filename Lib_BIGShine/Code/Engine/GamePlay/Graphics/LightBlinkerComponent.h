#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/EasingComponent.h"

class LightComponent;

class LightBlinkerComponent : public Component
{
public:
    // 点滅モード
    enum class Mode { Loop, PingPong, Once };

    void Start() override;

    void Update(float et) override;

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
    std::shared_ptr<LightComponent> light;
    std::shared_ptr< EasingComponent> easingComp;

    bool active = true;
    bool isTargetState = false;

    // 設定値
    EasingType type = EasingType::Lerp;
    float speed = 1.0f;

    float minSpeed = 0.5f;
    float maxSpeed = 2.0f;
    float intensityMultiplier = 0.0f; // 0にすると消灯⇔点灯の点滅になる
    float rangeMultiplier = 0.0f;
    bool useRangeBlink = false;

    // 保存用基準値
    float baseIntensity = 0.0f;
    float baseRange = 0.0f;

    // Easing管理用インデックス
    int intensityIdx = -1;
    int rangeIdx = -1;
};