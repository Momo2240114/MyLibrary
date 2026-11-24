#include "PrecompiledHeader.h"
#include "SceneTutorial.h"

SceneTutorial::SceneTutorial()
{
    sceneName = "TutorialScene";
    isEditorSuppressed = !SceneManager::IsEditorPlaySessionActive();
}

void SceneTutorial::Initialize()
{
    SceneBase::Initialize();

    if (isLoadingPreparation)
    {
        _GameObjectManager->LoadSceneAsync(sceneName, false);
    }
    else
    {
        _GameObjectManager->LoadScene(sceneName);
    }
}

bool SceneTutorial::RequiresFreeCursor() const
{
    return _GameObjectManager && _GameObjectManager->IsGameMenuPaused();
}
