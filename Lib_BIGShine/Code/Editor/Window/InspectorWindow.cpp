#include "InspectorWindow.h"

void InspectorWindow::Draw()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (BeginEditorWindow(nullptr, windowFlags)) 
    {
        DrawStylePopup();

        if (!gameObjectManager->GetSelectedObject())
        {
            EndEditorWindow();
            return;
        }

        gameObjectManager->DrawInspectorGui();
    }
    EndEditorWindow();
}
