#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

// JSONで配置した画面全体のSpriteを使い、暗転と完了後のActionを管理する。
class ScreenFadeComponent : public Component
{
public:
    // JSON復元で使用するComponent名を返す。
    const std::string& GetName() const override;
    // 読み込んだ初期設定からフェードを開始する。
    void Start() override;
    // Sceneの準備完了後だけフェード時間を進める。
    void Update(float deltaTime_) override;
    // 暗転完了後に実行するActionを予約する。
    bool BeginFadeOut(const std::string& action_);
    bool IsFadingOut() const { return fadingOut; }
    float GetFadeOutTime() const { return fadingOut ? elapsedTime : 0.0f; }
    // フェード設定を既存Inspectorから編集する。
    void Gui() override;
    // フェードの初期設定を保存する。
    void Serialize(nlohmann::json& json_) const override;
    // 旧JSONで欠けた設定には標準値を使って復元する。
    void Deserialize(const nlohmann::json& json_) override;
private:
    // Spriteの保存色は維持し、実行中の透明度だけを変更する。
    void ApplyAlpha(float alpha_);
    float fadeInDuration = 0.7f;
    float fadeOutDelay = 0.0f;
    float fadeOutDuration = 0.6f;
    bool fadeInOnStart = true;
    float elapsedTime = 0.0f;
    bool fadingOut = false;
    bool completionDispatched = false;
    bool sceneTransitionHandled = false;
    std::string completionAction;
};
