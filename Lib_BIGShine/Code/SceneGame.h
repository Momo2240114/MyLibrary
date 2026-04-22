#pragma once


#include"IScene.h"
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include"ShapeRenderer.h"

class SceneGame :public Scene
{
public:
	SceneGame()
	{
		std::string name = "GameScene";
		sceneName = name;
	}

	void Initialize();
	void Finalize();
	// XVˆ—
	void Update(float elapsedTime);

	// •`‰æˆ—
	void Render(float elapsedTime);

	void DrawGUI();

private:
	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;
};