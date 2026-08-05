#include "PrecompiledHeader.h"

#include "HierarchyWindow.h"

void HierarchyWindow::Draw()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    BeginEditorWindow(nullptr, windowFlags);
    DrawStylePopup();

    // Objectの親子構造や選択状態はGameObjectManager側が持つため、Hierarchy描画もManagerへ委譲する。
    gameObjectManager->DrawHierarchyGui();

    EndEditorWindow();
}
