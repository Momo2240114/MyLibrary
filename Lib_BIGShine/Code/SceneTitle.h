#pragma once

#include"IScene.h"
#include"ModelRenderSystem.h"
#include"SpriteRenderSystem.h"
#include"ShapeRenderer.h"


class SceneTitle :public Scene
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
private:
	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;
};