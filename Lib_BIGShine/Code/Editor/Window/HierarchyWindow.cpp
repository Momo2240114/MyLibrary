#include "PrecompiledHeader.h"

#include "HierarchyWindow.h"

void HierarchyWindow::Draw()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    // 非表示タブでは全Objectの階層走査を行わない。
    if (!BeginEditorWindow(nullptr, windowFlags))
    {
        EndEditorWindow();
        return;
    }
    DrawStylePopup();

    // Objectの親子構造や選択状態はGameObjectManager側が持つため、Hierarchy描画もManagerへ委譲する。
    gameObjectManager->DrawHierarchyGui();

    EndEditorWindow();
}
