#pragma once

#include"IScene.h"
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
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