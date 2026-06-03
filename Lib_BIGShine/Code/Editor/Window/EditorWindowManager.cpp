#include "EditorWindowManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"


void EditorWindowManager::DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Scene")) {
            // 状態を直接書き換える
            if (ImGui::MenuItem("Save Scene")) { gameObjectManager->GetSceneState().showSavePopup = true; }
            if (ImGui::MenuItem("Load Scene")) { gameObjectManager->GetSceneState().showLoadPopup = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            // 各窓の状態を切り替えるメニューを生成
            for (auto& window : windows) {
                bool visible = window->IsVisible();
                if (ImGui::MenuItem(window->GetName().c_str(), nullptr, &visible)) {
                    window->SetVisible(visible);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void EditorWindowManager::DrawAll()
{
    DrawMenuBar();
    // モーダルの描画を命じる
    gameObjectManager->DrawSceneModals();
    // 有効なWindowだけを描画する
    for (auto& window : windows) {
        if (window->IsVisible()) {
            window->Draw();
        }
    }
}
