#include "PrecompiledHeader.h"

#include "ShadowWindow.h"

void ShadowWindow::Draw()
{
    if (!isVisible)
    {
        return;
    }

    if (BeginEditorWindow("Shadow Settings"))
    {
        DrawStylePopup();
        // Shadowの具体的な項目はModelRenderSystem側に集約し、描画設定とUIのズレを防ぐ。
        modelRenderSystem.ShadowSettingGui();
    }

    EndEditorWindow();
}
