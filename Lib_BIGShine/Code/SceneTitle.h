#pragma once

#include"SceneBase.h"
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include"ShapeRenderer.h"


class SceneTitle :public SceneBase
{
public:
	SceneTitle()
	{
		std::string name = "TitleScene";
		sceneName = name;
	}

	void Initialize();
	void Finalize();
	void DrawGUI()override;
	// XVˆ—
	void Update(float elapsedTime);

	// •`‰æˆ—
	void Render(float elapsedTime);
};