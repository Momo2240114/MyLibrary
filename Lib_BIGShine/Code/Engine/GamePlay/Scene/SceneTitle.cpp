#include "PrecompiledHeader.h"
#include "SceneTitle.h"
#include "Engine/Debug/TracyProfiler.h"

SceneTitle::SceneTitle()
{
    sceneName = "TitleBackground";
    isLoadingPresentation = true;
}

void SceneTitle::Initialize()
{
    BS_PROFILE_SCOPE("SceneTitle::Initialize");
    SceneBase::Initialize();
    _GameObjectManager->LoadSceneAsync(sceneName);
}
