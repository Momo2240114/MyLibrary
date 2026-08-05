#pragma once

#include "EditorWindow.h"

// TimeScaleの変更や一時的なSlow/Stop演出をEditorから試すためのWindow。
class TimeManagerWindow : public EditorWindow
{
public:
    explicit TimeManagerWindow(GameObjectManager* manager_)
        : EditorWindow("Time Manager", manager_)
    {
        defaultWindowSize = ImVec2(400.0f, 440.0f);
    }

    void Draw() override;

private:
    // 入力された倍率・遷移時間をTimeManagerへ反映する。
    void ApplyTimeScale();

    // TimeScale変更に使う目標倍率と、補間/保持/復帰の時間設定。
    float targetScale = 1.0f;
    float defaultScale = 1.0f;
    float transitionSpeed = 10.0f;
    float transitionDuration = 0.2f;
    float holdDuration = 0.1f;
    float returnDuration = 0.2f;
};
