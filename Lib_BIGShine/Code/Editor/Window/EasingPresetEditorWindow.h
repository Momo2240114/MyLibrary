#pragma once

#include "EditorWindow.h"
#include "Engine/Math/Curve/CurveComponent.h"

// 0-1正規化されたEasing用CurveTrackを作成・保存するための専用Window。
class EasingPresetEditorWindow : public EditorWindow
{
public:
    explicit EasingPresetEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    // Curve編集UIと保存処理はCurveComponentに集約し、このWindowはEasing向け初期値だけを与える。
    CurveComponent curveEditor;
};
