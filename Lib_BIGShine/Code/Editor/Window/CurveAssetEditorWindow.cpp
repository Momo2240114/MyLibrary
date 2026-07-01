#include "CurveAssetEditorWindow.h"

CurveAssetEditorWindow::CurveAssetEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Curve Asset Editor", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(1040.0f, 640.0f);
}

void CurveAssetEditorWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }
    curveEditor.DrawEditorContent();
    DrawStylePopup();
    EndEditorWindow();
}
