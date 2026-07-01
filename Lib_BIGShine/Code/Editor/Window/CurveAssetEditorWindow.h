#pragma once

#include "EditorWindow.h"
#include "Engine/Math/Curve/CurveComponent.h"

class CurveAssetEditorWindow : public EditorWindow
{
public:
    explicit CurveAssetEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    CurveComponent curveEditor;
};
