#include "PrecompiledHeader.h"

#include "SceneStartUp.h"
#include "SceneGame.h"
#include "SceneTitle.h"
#include "SceneLoading.h"
#include "Engine/Core/BuildConfig.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Common/DebugLog.h"

#include <algorithm>
#include <imgui.h>
#include <memory>

void SceneStartUp::Initialize()
{
    BS_PROFILE_SCOPE("SceneStartUp::Initialize");

    // 起動時の状態を初期化し、最初のシーンへ入る前に必要なモデルの先読みを開始する。
    elapsedSeconds = 0.0f;
    titleSceneRequested = false;
    startupLoadFailed = false;
    SceneBase::Initialize();
    // 通常Debug構成でも、ゲーム専用起動かどうかで起動ブランドを揃える。
    _GameObjectManager->SetEngineLoadingPresentation(!GameRuntimeSettings::IsGameOnly());
    _GameObjectManager->LoadSceneAsync(sceneName);

    ModelResourceManager::Instance().LoadStartupResourceSettings();
    // Criticalは完了を待ち、HighPriorityは次のシーンでも非同期読み込みを継続する。
    criticalPreloadHandle = ModelResourceManager::Instance().RequestStartupCriticalModels();
    ModelResourceManager::Instance().RequestStartupHighPriorityModels();
    ModelResourceManager::Instance().StartAsyncLoader();
}

void SceneStartUp::Finalize()
{
    SceneBase::Finalize();
}

void SceneStartUp::Update(float elapsedTime_)
{
    BS_PROFILE_SCOPE("SceneStartUp::Update");

    elapsedSeconds += elapsedTime_;
    const float progress_ = criticalPreloadHandle.IsValid() ? criticalPreloadHandle.GetProgress() : 1.0f;
    _GameObjectManager->SetPresentationLoadingStatus({startupLoadFailed ? LOADING_STATE::FAILED : (!criticalPreloadHandle.IsValid() || criticalPreloadHandle.IsReady()) ? LOADING_STATE::COMPLETED : LOADING_STATE::LOADING,progress_,"Startup resources",startupLoadFailed ? "Required startup resources could not be loaded." : ""});
    SceneBase::Update(elapsedTime_);
    if (_GameObjectManager->GetSceneRuntimeState() == SCENE_RUNTIME_STATE::FAILED)
    {
        startupLoadFailed = true;
        _GameObjectManager->SetPresentationLoadingStatus({LOADING_STATE::FAILED,progress_,"Startup presentation failed",_GameObjectManager->GetSceneRuntimeError()});
        return;
    }

    // GPUアップロードはメインスレッドで、1フレーム2ms程度に抑えて少しずつ進める。
    ModelResourceManager::Instance().UpdateAsyncLoads(
        GraphicsDeviceManager::Instance().GetDevice(),
        2.0f);

    if (criticalPreloadHandle.IsValid() && criticalPreloadHandle.HasFailed())
    {
        // 必須モデルの読み込み失敗時は次のシーンへ進まず、GUIに失敗表示を出す。
        startupLoadFailed = true;
        BS_LOG_ERROR(LOG_CATEGORY::GAME, "[StartUpScene] Critical startup model failed to load.");
        return;
    }

    const bool criticalReady_ =
        !criticalPreloadHandle.IsValid() ||
        criticalPreloadHandle.IsReady();

    if (criticalReady_ && !titleSceneRequested && _GameObjectManager->GetSceneRuntimeState() == SCENE_RUNTIME_STATE::RUNNING)
    {
        // 遷移を一度だけ要求し、HighPriorityの先読みは次のシーンでも継続する。
        titleSceneRequested = true;
#if defined(BS_APP_ENGINE)
        if (!GameRuntimeSettings::IsGameOnly())
        {
            // Editor用EXEは既存のEditor遷移と同じロード経路で、MainSceneの編集状態を開く。
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::make_unique<SceneGame>()));
            return;
        }
#endif
        SceneManager::ChangeScene(std::make_unique<SceneTitle>());
    }
}

void SceneStartUp::Render(float elapsedTime_)
{
    SceneBase::Render(elapsedTime_);
}

// 起動の状態表示はLoadingPresentationComponentが描画する。
void SceneStartUp::DrawGUI() {}
