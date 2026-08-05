#include "PrecompiledHeader.h"

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
        // PostEffectは描画デバイス側の状態に依存するため、具体的な項目描画はGraphicsDeviceManagerへ委譲する。
        GraphicsDeviceManager::Instance().DrawPostEffectGuiContent();
    }
    EndEditorWindow();
}
