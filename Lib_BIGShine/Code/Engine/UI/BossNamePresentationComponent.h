#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include <string>

// 既存のUITextとSpriteRendererで、再利用可能なボス名のフェード表示を行う。
class BossNamePresentationComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static const std::string NAME = "BossNamePresentationComponent";
        return NAME;
    }
    // 透明状態を準備し、表示Actionを公開する。
    void Start() override;
    // FadeIn、表示保持、FadeOutの順に透明度を更新する。
    void Update(float deltaTime_) override;
    // 表示名とフェード時間を編集する。
    void Gui() override;
    // 表示設定を保存・復元し、古いデータには既定値を使用する。
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // ShowBossNameとHideBossNameをGlobal Actionとして登録する。
    void RegisterActions() override;
    // 名前を反映し、透明状態から表示を開始する。
    void ShowBossName();
    // 再生を止め、透明状態へ戻す。
    void HideBossName();
private:
    // 同じObject上のSpriteの透明度へ反映する。
    void ApplyAlpha(float alpha_);
    std::string bossName = "黒焔竜";
    std::string subName = "DRAGON NAME";
    float displayDuration = 1.2f;
    float fadeInDuration = 0.35f;
    float fadeOutDuration = 0.45f;
    float elapsed = 0.0f;
    bool playing = false;
};
