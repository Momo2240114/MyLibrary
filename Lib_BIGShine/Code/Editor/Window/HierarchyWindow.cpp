#include "HierarchyWindow.h"

void HierarchyWindow::Draw()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // --- 静的な変数で横幅を保持 ---
    static float leftPanelWidth = 300.0f;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, viewport->WorkSize.y));

    // スタイルの適用
    ImGui::PushStyleColor(ImGuiCol_WindowBg, style.backgroundColor);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, style.titleBarColor);

    ImGui::Begin(windowName.c_str());
    // --- ここで共通関数を呼び出す ---
    DrawStylePopup();


    gameObjectManager->DrawHierarchyGui();

    ImGui::End();

    // スタイルの変更の終了
    ImGui::PopStyleColor(2);
}