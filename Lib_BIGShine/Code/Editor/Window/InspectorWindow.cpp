#include "InspectorWindow.h"

void InspectorWindow::Draw()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    static float rightPanelWidth = 350.0f;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

    ImVec2 inspectorPos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightPanelWidth, viewport->WorkPos.y);

    ImGui::SetNextWindowPos(inspectorPos);
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, viewport->WorkSize.y));

    // スタイルの適用
    ImGui::PushStyleColor(ImGuiCol_WindowBg, style.backgroundColor);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, style.titleBarColor);

    // --- ウィンドウの描画 ---
    if (ImGui::Begin(windowName.c_str(), nullptr, windowFlags)) 
    {
        // --- ここで共通関数を呼び出す ---
        DrawStylePopup();

        // 配置調整のための横幅更新
        rightPanelWidth = ImGui::GetWindowWidth();

        if (!gameObjectManager->GetSelectedObject())
        {
            ImGui::End();
            // スタイルの変更の終了
            ImGui::PopStyleColor(2);
            return;
        }

        // 中身の描画（マネージャーから配置ロジックを削ったもの）
        gameObjectManager->DrawInspectorGui();
    }
    ImGui::End();

    // スタイルの変更の終了
    ImGui::PopStyleColor(2);
}
