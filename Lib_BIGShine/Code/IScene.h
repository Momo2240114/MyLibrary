#pragma once


#include "SceneManager.h"

#include"NavMeshManager.h"

#include "ActionRegistry.h"

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

	virtual NaviMeshManager* GetNaviMeshManager()
	{
		return m_pNaviMeshManager.get();
	}

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

	bool ready;

	std::shared_ptr<ActionRegistry> actionRegistry;
	std::unique_ptr<NaviMeshManager> m_pNaviMeshManager;

	std::string sceneName;

	bool isFirstUpdate = true;
};
