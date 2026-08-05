#include "PrecompiledHeader.h"

#include "InspectorWindow.h"

void InspectorWindow::Draw()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (BeginEditorWindow(nullptr, windowFlags))
    {
        DrawStylePopup();

        if (!gameObjectManager->GetSelectedObject())
        {
            // 選択Objectがない場合はInspectorに描画すべきComponentがないため、空Windowとして閉じる。
            EndEditorWindow();
            return;
        }

        // 選択ObjectのComponent編集UIはGameObjectManager経由でまとめて描画する。
        gameObjectManager->DrawInspectorGui();
    }
    EndEditorWindow();
}
