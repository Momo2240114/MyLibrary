#include "PrecompiledHeader.h"

#include "ShadowWindow.h"

void ShadowWindow::Draw()
{
    if (!isVisible) return;

    if (BeginEditorWindow("Shadow Settings"))
    {
        DrawStylePopup();
        modelRenderSystem.ShadowSettingGui();
    }

    EndEditorWindow();
}