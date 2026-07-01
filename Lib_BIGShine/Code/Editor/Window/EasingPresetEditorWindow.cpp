#include "EasingPresetEditorWindow.h"

EasingPresetEditorWindow::EasingPresetEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Easing Preset Editor", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(920.0f, 600.0f);
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
    curveEditor.DrawEditorContent();
    DrawStylePopup();
    EndEditorWindow();
}
