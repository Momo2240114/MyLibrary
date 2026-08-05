#pragma once

#include "EditorWindow.h"

class ModelRenderSystem;

// ModelRenderSystemが持つGPU描画調査用の設定をEditorから確認・変更するWindow。
class GpuRenderInvestigationWindow : public EditorWindow
{
public:
	GpuRenderInvestigationWindow(GameObjectManager* manager_, ModelRenderSystem& renderSystem_);

	void Draw() override;

private:
	// 実際の描画統計や調査設定はModelRenderSystem側が持つため、Windowは参照してGuiだけを呼び出す。
	ModelRenderSystem& modelRenderSystem;
};
