#pragma once

#include "EditorWindow.h"

class TimeManagerWindow : public EditorWindow
{
public:
    explicit TimeManagerWindow(GameObjectManager* manager_)
        : EditorWindow("Time Manager", manager_)
    {
        defaultWindowSize = ImVec2(360.0f, 360.0f);
    }

    void Draw() override;

private:
    void ApplyTimeScale();

    float targetScale = 1.0f;
    float transitionSpeed = 10.0f;
};
