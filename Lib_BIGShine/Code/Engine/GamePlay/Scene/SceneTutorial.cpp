#include "PrecompiledHeader.h"

#include "SceneTutorial.h"

void SceneTutorial::Finalize()
{
	// TutorialScene用の独自解放処理はまだ無いので、SceneBaseの終了処理に委譲する。
	SceneBase::Finalize();
}

void SceneTutorial::DrawGUI()
{
	// 現状は仮実装。TutorialScene固有GUIを追加する場合はSceneBase::DrawGUIへ差し替える。
	SceneBase::Finalize();
}


void SceneTutorial::Initialize()
{
	// 現状は仮実装。TutorialScene用のLoadScene処理を追加する入口。
	SceneBase::Finalize();
}

void SceneTutorial::Update(float elapsedTime)
{
	// 現状は仮実装。TutorialScene固有Updateを追加する入口。
	SceneBase::Finalize();
}

void SceneTutorial::Render(float elapsedTime)
{
	// 現状は仮実装。TutorialScene固有Renderを追加する入口。
	SceneBase::Finalize();
}


