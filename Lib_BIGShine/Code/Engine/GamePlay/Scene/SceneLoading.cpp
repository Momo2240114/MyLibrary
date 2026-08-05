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
		// LoadingSceneだけは軽量に見せたいので、Shape Debugを切ってロード画面を読み込む。
		_GameObjectManager->SetShapeRenderEnabled(false);
		_GameObjectManager->LoadScene("LoadingScene");
	}

	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());

	//スレッド開始
	// nextSceneのInitializeは重いので、LoadingSceneを表示しながら別スレッドで進める。
	thread = std::make_unique<std::thread>(LoadingThread, this);
}

void SceneLoading::Finalize()
{
	m_SpriteRenderSystem.Finalize();


	if (thread && thread->joinable()) {
		// 次シーン初期化中にSceneLoadingが破棄されても、スレッド終了を待って安全に閉じる。
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
		// 初期化済みのSceneをそのまま渡すため、ChangeScene側で再Initializeしない。
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
	// WICなどCOMを使うリソース読み込みがあるため、ロードスレッド側でも初期化する。
	CoInitialize(nullptr);

	//次のシーンの初期化を行う
	// ここでモデルやSceneObjectを作り、完了後にReadyフラグを立てる。
	scene->nextScene->Initialize();

	//スレッドが終わる前にCOM関連の終了化
	CoUninitialize();

	//次のシーンの準備完了設定
	scene->nextScene->SetReady();
}
