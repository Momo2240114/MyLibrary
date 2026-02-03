#pragma once

//基底クラス
#include "IScene.h"
//レンダー用のシステム
#include"ModelRenderSystem.h"
#include"SpriteRenderSystem.h"
#include"GameObjectManager.h"

#include <thread>
#include "GlTFModel.h"

#include"GameObject.h"

//ローディングシーン
class SceneLoading : public Scene
{
public:
	SceneLoading(std::unique_ptr<Scene> next) : nextScene(std::move(next)) {}
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
	std::unique_ptr<Scene> nextScene;
	std::unique_ptr<std::thread> thread;
};

