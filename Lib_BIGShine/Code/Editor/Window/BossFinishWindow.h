#pragma once
#include "EditorWindow.h"

// 本番と同じDirectorとJSONを使用し、構図と独立したポーズ・VFX時刻を編集する。
class BossFinishWindow : public EditorWindow
{
public:
    explicit BossFinishWindow(GameObjectManager* manager_) : EditorWindow("ANATHEMA Boss Finish", manager_)
    {
        isVisible = false;
        defaultWindowSize = ImVec2(640, 800);
    }

    ~BossFinishWindow() override;
    void Draw() override;
    void OnVisibilityChanged(bool visible_) override;

private:
    // 実在する保存項目を直接編集し、未実装の装飾UIは作らない。
    void DrawSettings(nlohmann::json& value_, const std::string& label_);
};
