#pragma once

#include "EditorWindow.h"

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
    void ApplyTimeScale();

    float targetScale = 1.0f;
    float defaultScale = 1.0f;
    float transitionSpeed = 10.0f;
    float transitionDuration = 0.2f;
    float holdDuration = 0.1f;
    float returnDuration = 0.2f;
};
