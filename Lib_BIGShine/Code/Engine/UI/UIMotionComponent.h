#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "TransitionParameters.h"
class UIWidgetComponent;

// Sprite・文字・Widgetへ加える描画専用の補正値。保存Transformや色を変更しない。
struct UIMotionSample
{
    DirectX::XMFLOAT2 offset{0,0};
    DirectX::XMFLOAT2 scale{1,1};
    float rotation = 0;
    float opacity = 1;
};

// キーフレーム型UIAnimationPlayerと併用できる、プリセット型の汎用UIモーション。
class UIMotionComponent : public Component
{
public:
    const std::string& GetName() const override;
    // JSONと同じOwner上のActionを準備し、実行中なら自動再生する。
    void Start() override;
    void OnDestroy() override;
    // ポーズに依存しない時計で補正値を評価する。
    void UpdateUnscaled(float deltaTime_) override;
    // プリセット、時間、補間と補正量を編集して保存する。
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // 明示的な試写はEditorでも生時間で進める。
    void Play(bool preview_ = false);
    void Stop(bool restore_ = true);

    void Pause()
    {
        paused = true;
    }

    void Resume()
    {
        paused = false;
    }

    bool LoadPreset(const std::string& name_);

    const UIMotionSample& GetSample() const
    {
        return sample;
    }

private:
    // 設定を評価し、保存値から独立した描画補正を更新する。
    void Evaluate();
    void ApplyWidget();
    nlohmann::json settings = nlohmann::json::object();
    nlohmann::json catalog = nlohmann::json::object();
    std::string preset = "PopIn";
    std::string saveName = "CustomMotion";
    UIMotionSample sample;
    std::weak_ptr<UIWidgetComponent> widget;
    float elapsedTime = 0;
    bool playing = false;
    bool paused = false;
    bool preview = false;
    bool playOnStart = true;
    std::string status;
};
