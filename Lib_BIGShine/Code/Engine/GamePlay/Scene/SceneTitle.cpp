#include "PrecompiledHeader.h"

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


	// TitleSceneでは現在MainSceneを読み込み、タイトル用の初期シーンとして扱う。
	_GameObjectManager->LoadScene("MainScene");

	// Sceneロード後に親子関係を解決し、Transform階層を使える状態にする。
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

