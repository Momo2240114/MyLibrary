#include "PrecompiledHeader.h"

#include "GpuRenderInvestigationWindow.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"

#include <imgui.h>

GpuRenderInvestigationWindow::GpuRenderInvestigationWindow(
	GameObjectManager* manager_,
	ModelRenderSystem& renderSystem_)
	: EditorWindow("GPU Render Investigation", manager_),
	modelRenderSystem(renderSystem_)
{
	defaultWindowSize = ImVec2(430.0f, 320.0f);
}

void GpuRenderInvestigationWindow::Draw()
{
	if (!isVisible) return;

	if (BeginEditorWindow())
	{
		DrawStylePopup();
		modelRenderSystem.RenderInvestigationSettingGui();
	}
	EndEditorWindow();
}
