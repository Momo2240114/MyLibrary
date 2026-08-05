#pragma once

#include "EditorWindow.h"
#include "Engine/Math/Curve/CurveComponent.h"

// 汎用CurveTrack AssetをEditor上で作成・調整するWindow。
class CurveAssetEditorWindow : public EditorWindow
{
public:
    explicit CurveAssetEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    // Assetとして保存できるCurve編集UIを保持する。
    CurveComponent curveEditor;
};
