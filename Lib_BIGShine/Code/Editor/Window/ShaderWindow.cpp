#include "PrecompiledHeader.h"

// ShaderWindow.cpp
#include "ShaderWindow.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"

#include <imgui.h>

void ShaderWindow::Draw() {
    if (!isVisible) return;


    if (BeginEditorWindow()) {
        DrawStylePopup();
        // Shader本体の切り替え設定とOutline設定をまとめて表示し、描画結果の確認をしやすくする。
        modelRenderSystem.ShaderSettingGui();
        modelRenderSystem.OutLineSettingGui();
    }
    EndEditorWindow();
}
