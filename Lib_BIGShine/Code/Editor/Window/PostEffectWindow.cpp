#include "PostEffectWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

void PostEffectWindow::Draw()
{
    if (!isVisible)
    {
        return;
    }

    if (BeginEditorWindow())
    {
        DrawStylePopup();
        GraphicsDeviceManager::Instance().DrawPostEffectGuiContent();
    }
    EndEditorWindow();
}
