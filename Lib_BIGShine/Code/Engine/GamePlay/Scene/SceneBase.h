#pragma once


#include"IScene.h"
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include"Engine/Graphics/RenderSystem/ShapeRenderer.h"

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

	// SceneView ドッキング中は専用 RenderTarget、通常時はバックバッファへ描画する。
	void Render(float elapsedTime);

	// シーン状態表示と EditorWindow 群の ImGui 描画をまとめて行う。
	void DrawGUI();

	std::shared_ptr<GameObjectManager> GetGameObjectManager();
protected:
	template<class T>
	void ChangeScene()
	{
		// 派生 Scene が型だけ指定して次の Scene へ切り替えられるようにする補助。
		SceneManager::ChangeScene(std::make_unique<T>());
	}

	// エディタの各 Window を束ねる管理クラス。GameObjectManager を参照して編集する。
	std::unique_ptr<EditorWindowManager> editorWindowManager;
	bool ready;

	// Animation/Event/Component から呼ばれる Action のグローバル登録先。
	std::shared_ptr<ActionRegistry> actionRegistry;
	std::string sceneName;
	bool isFirstUpdate = true;
	// 窓の表示状態を管理する変数（クラスメンバとして保持すること）
	bool isEditorVisible = true;

	// Scene 内で共有する描画システム。GameObjectManager::RenderAll へ渡して一括描画する。
	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;

	// Scene が所有する GameObject 群と各種 Runtime System の中心。
	std::shared_ptr<GameObjectManager> _GameObjectManager;
};
