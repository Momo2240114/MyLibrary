#pragma once
#include <atomic>


#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

// インターフェイスシーン
class Scene
{
public:
	Scene() = default;
	virtual ~Scene() = default;

	virtual void Initialize() {};
	virtual void  Finalize() {};

	// 更新処理
	virtual void Update(float elapsedTime) {}

	// 描画処理
	virtual void Render(float elapsedTime) {}

	// GUI描画処理
	virtual void DrawGUI() {}

    // タイトルなどマウスで操作するSceneは、ゲーム専用起動でもカーソルを解放する。
    virtual bool RequiresFreeCursor() const { return false; }

	//準備完了しているか
	bool IsReady()const { return ready; }

	//準備完了設定
	void SetReady() { ready = true; }

	std::string GetSceneName() {return sceneName;}
protected:
	template<class T>
	void ChangeScene()
	{
		SceneManager::ChangeScene(std::make_unique<T>());
	}

	// ロードスレッドの初期化結果を、Scene切替側へ確実に公開する。
	std::atomic<bool> ready{false};

	std::shared_ptr<ActionRegistry> actionRegistry;

	std::string sceneName;

	bool isFirstUpdate = true;
};
