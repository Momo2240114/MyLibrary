#include "PrecompiledHeader.h"

#include "SceneLoading.h"

#include"Engine/Graphics/Core/GraphicsDeviceManager.h"

#include"Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include"Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <format>

void SceneLoading::Initialize()
{
	SceneBase::Initialize();

	if (_GameObjectManager) {
		_GameObjectManager->SetShapeRenderEnabled(false);
		_GameObjectManager->LoadScene("LoadingScene");
	}

	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());

	//スレッド開始
	thread = std::make_unique<std::thread>(LoadingThread, this);
}

void SceneLoading::Finalize()
{
	m_SpriteRenderSystem.Finalize();


	if (thread && thread->joinable()) {
		thread->join(); // スレッドの終了を待機
	}
}

void SceneLoading::Update(float elapsedTime)
{
	SceneBase::Update(elapsedTime);

	// ★自身のマネージャーを更新
	if (_GameObjectManager) {
		_GameObjectManager->Update(elapsedTime);
	}

	if (nextScene && nextScene->IsReady())
	{
		auto tempScene = std::move(nextScene);
		SceneManager::ChangeScene(std::move(tempScene), false);
		return;
	}
}

void SceneLoading::Render(float elapsedTime)
{
	SceneBase::Render(elapsedTime);
}

void SceneLoading::DrawGUI()
{
	SceneBase::DrawGUI();
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
