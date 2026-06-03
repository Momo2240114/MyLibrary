#pragma once

//基底クラス
#include "SceneBase.h"
//レンダー用のシステム
#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include"Engine/GamePlay/Core/Object/GameObjectManager.h"

#include <thread>
#include "Engine/Graphics/Resource/GlTFModel.h"

#include"Engine/GamePlay/Core/Object/GameObject.h"

//ローディングシーン
class SceneLoading : public SceneBase
{
public:
	SceneLoading(std::unique_ptr<SceneBase> next) : nextScene(std::move(next)) {}
	~SceneLoading()override {}

	//初期化
	void Initialize() override;

	//終了化
	void Finalize()override;

	//更新処理
	void Update(float elapsedTime)override;

	//描画処理
	void Render(float elapsedTime) override;
	//GUI描画
	void DrawGUI()override;

private:
	//ローディングスレッド
	static void LoadingThread(SceneLoading* scene);

	// ロード画面専用のオブジェクトリスト
	std::vector<std::shared_ptr<GameObject>> m_LoadingObjects;

	// ロード画面専用のカメラ（GameObjectとして持っておくと管理が楽です）
	std::shared_ptr<GameObject> m_LoadingCamera;

	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;

	// 生ポインタではなく unique_ptr で保持する
	std::unique_ptr<SceneBase> nextScene;
	std::unique_ptr<std::thread> thread;
};

