#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "MenuPresentation.h"

// 通常Sceneの二択メニューを、ポーズ中も動く汎用UIとして提供する。
class GameMenuComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void OnDestroy() override;
    void UpdateUnscaled(float deltaTime_) override;
    // SceneのSprite描画後にメニューを重ねる。
    void RenderMenu(SpriteRenderSystem& sprites_);
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void Open();
    void Close();

    bool IsOpen() const
    {
        return active;
    }

    bool IsPausingWorld() const
    {
        return active && pauseWorld;
    }

private:
    MenuPresentation menu;
    std::string title = "PAUSED";
    std::string firstLabel = "再開";
    std::string secondLabel = "タイトルに戻る";
    std::string firstAction;
    std::string secondAction = "GoSceneTitle";
    bool pauseWorld = true;
    bool active = false;
    bool submitted = false;
    bool previousInputCapture = false;
    bool previousCursorLock = false;
    std::string toggleInputAction = "UI.ToggleGameMenu";
};
