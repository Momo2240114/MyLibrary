#pragma once

#include "EditorWindow.h"
#include "Engine/Math/Curve/CurveComponent.h"

class EasingPresetEditorWindow : public EditorWindow
{
public:
    explicit EasingPresetEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    CurveComponent curveEditor;
};
