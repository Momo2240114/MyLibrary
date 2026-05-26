#pragma once


#include"IScene.h"
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include"ShapeRenderer.h"

#include"Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Editor/Window/EditorWindowManager.h"

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
	// 更新処理
	virtual void Update(float elapsedTime);

	// 描画処理
	void Render(float elapsedTime);

	void DrawGUI();

	std::shared_ptr<GameObjectManager> GetGameObjectManager();
protected:
	template<class T>
	void ChangeScene()
	{
		SceneManager::ChangeScene(std::make_unique<T>());
	}

	EditorWindowManager editorWindowManager;
	bool ready;
	std::shared_ptr<ActionRegistry> actionRegistry;
	std::string sceneName;
	bool isFirstUpdate = true;
	// 窓の表示状態を管理する変数（クラスメンバとして保持すること）
	bool isEditorVisible = true;
	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;
	std::shared_ptr<GameObjectManager> _GameObjectManager;
};