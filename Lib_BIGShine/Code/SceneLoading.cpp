#include "SceneLoading.h"

#include"Engine/Graphics/Core/GraphicsDeviceManager.h"

#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <format>

void SceneLoading::Initialize()
{
	GameObjectManager::Instance().SetShapeRenderFlg(false);
	GameObjectManager::LoadingInstance().SetShapeRenderFlg(false);

	GameObjectManager::LoadingInstance().Initialize();
	GameObjectManager::LoadingInstance().LoadScene("LoadingScene");


	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());

	//スレッド開始
	thread = std::make_unique<std::thread>(LoadingThread, this);
}

void SceneLoading::Finalize()
{
	m_SpriteRenderSystem.Finalize();
	GameObjectManager::LoadingInstance().Clear();

	if (thread && thread->joinable()) {
		thread->join(); // スレッドの終了を待機
	}
}

void SceneLoading::Update(float elapsedTime)
{
	GameObjectManager::LoadingInstance().Update(elapsedTime);
	// nextScene自体が空でないこともチェック
	if (nextScene && nextScene->IsReady())
	{
		// 確実にポインタを渡す
		auto tempScene = std::move(nextScene);
		SceneManager::ChangeScene(std::move(tempScene), false);
		// これ以降、この関数内で nextScene に触れてはいけない
		return;
	}
}

void SceneLoading::Render(float elapsedTime)
{
	GameObjectManager::LoadingInstance().RenderAll(m_SpriteRenderSystem,m_ModelRenderSystem);
}

void SceneLoading::DrawGUI()
{
}

void SceneLoading::LoadingThread(SceneLoading* scene)
{
	//COM関連の初期化でスレッド毎に呼び必要がある
	CoInitialize(nullptr);

	//次のシーンの初期化を行う
	scene->nextScene->Initialize();

	//スレッドが終わる前にCOM関連の終了化
	CoUninitialize();

	//次のシーンの準備完了設定
	scene->nextScene->SetReady();
}
