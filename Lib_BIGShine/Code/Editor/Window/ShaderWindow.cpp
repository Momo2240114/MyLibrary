// ShaderWindow.cpp
#include "ShaderWindow.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"

#include <imgui.h>

void ShaderWindow::Draw() {
    if (!isVisible) return;

    // スタイルの適用
    ImGui::PushStyleColor(ImGuiCol_WindowBg, style.backgroundColor);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, style.titleBarColor);

    if (ImGui::Begin(windowName.c_str(), &isVisible)) {
        // スタイル変更用ポップアップ
        DrawStylePopup();
        modelRenderSystem.Gui();
    }
    ImGui::End();

    // スタイル終了
    ImGui::PopStyleColor(2);
}