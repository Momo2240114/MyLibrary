#pragma once
#include "EditorWindow.h"

// HierarchyWindow.h
class InspectorWindow : public EditorWindow {
public:
    InspectorWindow(GameObjectManager* manager) : EditorWindow("InspectorWindow", manager) {}

    void Draw() override;
};

