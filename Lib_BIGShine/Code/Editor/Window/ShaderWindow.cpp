#include "PrecompiledHeader.h"

// ShaderWindow.cpp
#include "ShaderWindow.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"

#include <imgui.h>

void ShaderWindow::Draw() {
    if (!isVisible) return;


    if (BeginEditorWindow()) {
        DrawStylePopup();
        modelRenderSystem.ShaderSettingGui();
        modelRenderSystem.OutLineSettingGui();
    }
    EndEditorWindow();
}