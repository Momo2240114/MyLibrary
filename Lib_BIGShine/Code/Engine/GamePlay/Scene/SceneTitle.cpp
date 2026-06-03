#include "SceneTitle.h"


void SceneTitle::Finalize()
{
	SceneBase::Finalize();
}

void SceneTitle::DrawGUI()
{
	SceneBase::DrawGUI();


}


void SceneTitle::Initialize()
{
	SceneBase::Initialize();


	_GameObjectManager->LoadScene("MainScene");

	_GameObjectManager->ResolveHierarchy();
}

void SceneTitle::Update(float elapsedTime)
{
	SceneBase::Update(elapsedTime);
}

void SceneTitle::Render(float elapsedTime)
{
	SceneBase::Render(elapsedTime);
}

