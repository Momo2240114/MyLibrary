#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <memory>
#include <string>

class UIWidgetComponent;
class SpriteRenderer;

enum class UI_PROGRESS_SOURCE
{
    // Progress値の取得元。手動指定か、GameModeManagerのHP割合を参照する。
    MANUAL,
    PLAYER_HP_RATE,
    BOSS_HP_RATE
};

class UIProgressBarComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    const std::string& GetName() const override
    {
        static std::string name_ = "UIProgressBarComponent";
        return name_;
    }

    // 外部コードから手動Target Rateを設定する。
    void SetTargetRate(float targetRate_);

private:
    // IDで指定された子Widget/SpriteをScene内から探す。
    void ResolveWidgets();
    // 現在Rate、DelayRate、Effect用WidgetのLayoutをまとめて反映する。
    void ApplyLayoutRates();
    // Widgetの横Anchorをrateに合わせ、Barの表示幅を変える。
    void ApplyRateToWidget(UIWidgetComponent* widget_, float rate_);
    void ApplyBackgroundLayout();
    void ApplyFlashLayout();
    void ApplyEdgeHighlightLayout();
    void ApplyEffectVisuals();
    // Source設定から現在目標値を取得する。
    float ResolveTargetRate();
    // currentRateをtargetRateへ一定速度で近づける。
    float MoveRate(float currentRate_, float targetRate_, float speed_, float deltaTime_) const;

private:
    // 構成に使う各WidgetのID。
    std::string backgroundWidgetId;
    std::string currentWidgetId;
    std::string damageDelayWidgetId;
    std::string healDelayWidgetId;
    std::string flashWidgetId;
    std::string edgeHighlightWidgetId;

    // 値の取得元と、各Layerの表示Rate。
    UI_PROGRESS_SOURCE source = UI_PROGRESS_SOURCE::MANUAL;
    float targetRate = 1.0f;
    float currentRate = 1.0f;
    float damageDelayRate = 1.0f;
    float healDelayRate = 1.0f;
    // ダメージ遅延/回復追従の速度。
    float decreaseDelaySpeed = 0.45f;
    float increaseDelaySpeed = 0.65f;
    float flashDuration = 0.28f;
    float flashTimer = 0.0f;
    // 現在値の端に出すHighlightの幅とAlpha。
    float edgeHighlightWidth = 8.0f;
    float edgeHighlightBaseAlpha = 0.65f;
    DirectX::XMFLOAT4 damageFlashColor = { 1.0f, 0.2f, 0.08f, 0.75f };
    DirectX::XMFLOAT4 healFlashColor = { 1.0f, 1.0f, 1.0f, 0.55f };
    DirectX::XMFLOAT4 edgeHighlightColor = { 1.0f, 0.88f, 0.7f, 1.0f };
    DirectX::XMFLOAT4 activeFlashColor = damageFlashColor;

    // ID解決済みWidget/Sprite。所有は各GameObject側。
    std::weak_ptr<UIWidgetComponent> currentWidget;
    std::weak_ptr<UIWidgetComponent> backgroundWidget;
    std::weak_ptr<UIWidgetComponent> damageDelayWidget;
    std::weak_ptr<UIWidgetComponent> healDelayWidget;
    std::weak_ptr<UIWidgetComponent> flashWidget;
    std::weak_ptr<UIWidgetComponent> edgeHighlightWidget;
    std::weak_ptr<SpriteRenderer> flashSprite;
    std::weak_ptr<SpriteRenderer> edgeHighlightSprite;
};
