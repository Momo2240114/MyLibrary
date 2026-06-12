#pragma once
#include "EditorWindow.h"

// HierarchyWindow.h
class InspectorWindow : public EditorWindow {
public:
    InspectorWindow(GameObjectManager* manager) : EditorWindow("InspectorWindow", manager) 
    {
        defaultWindowSize = ImVec2(700.0f, 800.0f);
    }

    void Draw() override;
};

