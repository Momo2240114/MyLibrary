#pragma once

#include "EditorWindow.h"

class TimeManagerWindow : public EditorWindow
{
public:
    explicit TimeManagerWindow(GameObjectManager* manager_)
        : EditorWindow("Time Manager", manager_)
    {
        defaultWindowSize = ImVec2(320.0f, 180.0f);
    }

    void Draw() override;

private:
    void ApplyTimeScale();

    float targetScale = 1.0f;
    float transitionSpeed = 10.0f;
};
