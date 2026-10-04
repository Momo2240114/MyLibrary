#pragma once

#include "EditorWindow.h"

#include <array>
#include <string>

// HDRIとIBL強度を実行中に調整するEditor Window
class IBLSettingsWindow : public EditorWindow
{
public:
    explicit IBLSettingsWindow(GameObjectManager* manager_);

    // GraphicsDeviceManagerへHDRI設定を反映する
    void Draw() override;
    void OnVisibilityChanged(bool visible_) override;

private:
    // GraphicsDeviceManagerが保持している現在値をUI編集用へ同期する
    void SyncFromGraphicsSettings();

private:
    std::array<char, 260> hdrPathBuffer{};
    float hdrIntensity = 1.0f;
    bool hasSyncedFromGraphics = false;
    std::string statusMessage;
};
