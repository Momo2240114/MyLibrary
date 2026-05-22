#pragma once


#include"IScene.h"
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include"ShapeRenderer.h"

#include"Engine/GamePlay/Core/Object/GameObjectManager.h"

class SceneBase :public Scene
{
public:
	SceneBase()
	{
		std::string name = "SceneBase";
		sceneName = name;
	}

	virtual void Initialize();
	virtual void Finalize();
	// XVˆ—
	virtual void Update(float elapsedTime);

	// •`‰æˆ—
	void Render(float elapsedTime);

	void DrawGUI();

	std::shared_ptr<GameObjectManager> GetGameObjectManager();
protected:
	template<class T>
	void ChangeScene()
	{
		SceneManager::ChangeScene(std::make_unique<T>());
	}


	bool ready;
	std::shared_ptr<ActionRegistry> actionRegistry;
	std::string sceneName;
	bool isFirstUpdate = true;

	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;
	std::shared_ptr<GameObjectManager> _GameObjectManager;
};