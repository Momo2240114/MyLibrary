#include "HierarchyWindow.h"

void HierarchyWindow::Draw()
{
    ImGui::Begin(windowName.c_str());

    gameObjectManager->Gui();

    ImGui::End();
}