#include "PrecompiledHeader.h"

#include "EasingPresetEditorWindow.h"

EasingPresetEditorWindow::EasingPresetEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Easing Preset Editor", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(920.0f, 600.0f);
    // Easingは入力時間0-1に対して出力値0-1を返す前提なので、最初は線形Trackで開始する。
    curveEditor.CreateLinearTrack("Easing", 0.0f, 1.0f);
}

void EasingPresetEditorWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }
    ImGui::TextDisabled("Create a normalized 0-1 easing curve and save it as a CurveTrack asset.");
    // 実際のキー編集・Preview・保存UIは汎用CurveComponent側で描画する。
    curveEditor.DrawEditorContent();
    DrawStylePopup();
    EndEditorWindow();
}
