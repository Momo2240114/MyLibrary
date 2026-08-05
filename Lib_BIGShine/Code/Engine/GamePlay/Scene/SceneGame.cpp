#include "PrecompiledHeader.h"

#include"SceneGame.h"

void SceneGame::Finalize()
{
	SceneBase::Finalize();
}

void SceneGame::Initialize()
{
	SceneBase::Initialize();


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
