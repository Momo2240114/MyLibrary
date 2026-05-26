#pragma once
#include "EditorWindow.h"

// HierarchyWindow.h
class HierarchyWindow : public EditorWindow {
public:
    HierarchyWindow(GameObjectManager* manager) : EditorWindow("Hierarchy", manager) {}

    void Draw() override;
};