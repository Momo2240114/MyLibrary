#pragma once

#include "EditorWindow.h"

class ModelRenderSystem;

class GpuRenderInvestigationWindow : public EditorWindow
{
public:
	GpuRenderInvestigationWindow(GameObjectManager* manager_, ModelRenderSystem& renderSystem_);

	void Draw() override;

private:
	ModelRenderSystem& modelRenderSystem;
};
