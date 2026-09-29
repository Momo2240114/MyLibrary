#include "PrecompiledHeader.h"

#include"SceneGame.h"

#include "Game/GameMode/GameModeManager.h"

void SceneGame::Finalize()
{
	SceneBase::Finalize();
}

void SceneGame::Initialize()
{
	SceneBase::Initialize();

	_GameObjectManager->LoadScene("MainScene");
	if (GameModeManager* gameModeManager_ = _GameObjectManager->GetGameModeManager())
	{
		// Sceneロード後、ComponentとAction登録が揃い次第GameModeManager側でBossIntroを開始する。
		gameModeManager_->BeginGameSceneFlow();
	}

	// SceneBase側で作られたGameObjectManagerの階層関係を確定させる。
	_GameObjectManager->ResolveHierarchy();
}

void SceneGame::Update(float elapsedTime)
{
	SceneBase::Update(elapsedTime);
}

void SceneGame::Render(float elapsedTime)
{
	SceneBase::Render(elapsedTime);
}

void SceneGame::DrawGUI()
{
	SceneBase::DrawGUI();
}
