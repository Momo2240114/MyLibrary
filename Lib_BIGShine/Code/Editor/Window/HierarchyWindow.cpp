#include "PrecompiledHeader.h"

#include "HierarchyWindow.h"

void HierarchyWindow::Draw()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    BeginEditorWindow(nullptr, windowFlags);
    DrawStylePopup();

    gameObjectManager->DrawHierarchyGui();

    EndEditorWindow();
}