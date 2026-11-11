#include "PrecompiledHeader.h"
#include "SceneLoading.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include <algorithm>
#include <imgui.h>

SceneLoading::SceneLoading(std::unique_ptr<SceneBase> next_) : nextScene(std::move(next_))
{
    sceneName = "LoadingPresentation";
    isLoadingPresentation = true;
    enginePresentation = nextScene && nextScene->IsEditorDestination() && !GameRuntimeSettings::IsGameOnly();
}

SceneLoading::~SceneLoading() = default;

// 表示SceneのResourceが揃うまで次Sceneの初期化を始めず、表示側の初回停止を分離する。
void SceneLoading::Initialize()
{
    BS_PROFILE_SCOPE("SceneLoading::Initialize");
    loadingStatus = {LOADING_STATE::PREPARING, 0.0f, "Prepare loading presentation", ""};
    nextInitialized = false;
    transferRequested = false;
    transferQueued = false;
    completedTime = 0.0f;
    try
    {
        SceneBase::Initialize();
        _GameObjectManager->SetShapeRenderEnabled(false);
        _GameObjectManager->SetEngineLoadingPresentation(enginePresentation);
        _GameObjectManager->SetPresentationRetryEnabled(true);
        _GameObjectManager->LoadSceneAsync(sceneName);
    }
    catch (const std::exception& exception_) { FailLoading(exception_.what()); }
}

// 破棄対象のScene自身のActionRegistryを有効にしてから、Componentの解除を実行する。
void SceneLoading::Finalize()
{
    BS_PROFILE_SCOPE("SceneLoading::Finalize");
    if (nextScene && nextInitialized)
    {
        nextScene->ActivateContext();
        nextScene->Finalize();
        nextInitialized = false;
    }
    SceneBase::Finalize();
}

void SceneLoading::SetCompletionHoldDuration(float duration_)
{
    completionHoldDuration = (std::max)(0.0f, duration_);
}

void SceneLoading::RequestTransition()
{
    if (loadingStatus.IsCompleted()) transferRequested = true;
}

void SceneLoading::FailLoading(const std::string& error_)
{
    loadingStatus.state = LOADING_STATE::FAILED;
    loadingStatus.operation = "Scene load failed";
    loadingStatus.error = error_;
}

// 失敗した次Sceneを主スレッドで解放し、表示側は維持して同じ要求を再試行する。
void SceneLoading::Retry()
{
    if (!loadingStatus.HasFailed() || !nextScene) return;
    if (nextInitialized)
    {
        nextScene->ActivateContext();
        nextScene->Finalize();
    }
    nextInitialized = false;
    completedTime = 0.0f;
    loadingStatus = {LOADING_STATE::PREPARING, 0.0f, "Retry scene load", ""};
    ActivateContext();
}

void SceneLoading::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("SceneLoading::Update");
    if (!_GameObjectManager) return;
    try
    {
        ActivateContext();
        if (_GameObjectManager->TakePresentationRetryRequest()) Retry();
        _GameObjectManager->SetPresentationLoadingStatus(loadingStatus);
        _GameObjectManager->SetPresentationWarning(presentationWarning);
        _GameObjectManager->SetPresentationSilhouetteOverride(silhouetteModeOverridden,silhouetteMode == LOADING_SILHOUETTE_MODE::WHITE);
        _GameObjectManager->Update(deltaTime_);
        // 移譲を予約した後も、閉じる演出が完了するまでシルエットを動かす。
        if (transferQueued) return;
        if (SceneManager::IsRestoringEditorScene() ||
            _GameObjectManager->GetEditorPlayMode() == EditorPlayMode::PAUSED) return;
        if (loadingStatus.HasFailed()) return;
        const auto displayState_ = _GameObjectManager->GetSceneRuntimeState();
        if (displayState_ == SCENE_RUNTIME_STATE::FAILED)
        {
            // 表示Assetの失敗はプレーンなUIへフォールバックし、次Sceneの読み込みは継続する。
            presentationWarning = _GameObjectManager->GetSceneRuntimeError();
        }
        else if (displayState_ != SCENE_RUNTIME_STATE::RUNNING) return;
        if (!nextScene) { FailLoading("No target scene was supplied."); return; }
        if (!nextInitialized)
        {
            nextScene->SetLoadingPreparation(true);
            nextInitialized = true;
            nextScene->Initialize(); // GameObject・Device操作は必ず主スレッド。
            loadingStatus.state = LOADING_STATE::LOADING;
            loadingStatus.operation = "Read target scene";
            ActivateContext();
            return;
        }
        const auto manager_ = nextScene->GetGameObjectManager();
        if (!manager_) { FailLoading("Target scene has no object manager."); return; }
        if (!loadingStatus.IsCompleted())
        {
            nextScene->ActivateContext();
            manager_->AdvanceSceneLoading(deltaTime_);
            ActivateContext();
            const auto state_ = manager_->GetSceneRuntimeState();
            loadingStatus.progress = std::clamp(manager_->GetSceneLoadProgress(), 0.0f, 1.0f);
            loadingStatus.operation = manager_->GetSceneLoadOperation();
            if (state_ == SCENE_RUNTIME_STATE::FAILED) { FailLoading(manager_->GetSceneRuntimeError()); return; }
            loadingStatus.state = state_ == SCENE_RUNTIME_STATE::INITIALIZING ? LOADING_STATE::FINALIZING : LOADING_STATE::LOADING;
            if (state_ == SCENE_RUNTIME_STATE::RUNNING)
            {
                loadingStatus = {LOADING_STATE::COMPLETED, 1.0f, "Ready", ""};
                completedTime = 0.0f;
            }
            return; // 完了したフレームも必ずロード画面を1回描画する。
        }
        completedTime += deltaTime_;
        if (automaticTransition && completedTime >= completionHoldDuration) RequestTransition();
        if (transferRequested && !transferQueued)
        {
            transferQueued = true;
            nextScene->SetLoadingPreparation(false);
            nextScene->SetReady();
            SceneManager::ChangeScene(std::move(nextScene), false);
        }
    }
    catch (const std::exception& exception_) { ActivateContext(); FailLoading(exception_.what()); }
}

// 通常Sceneと同じ描画経路で、JSONの演出設定と専用Cameraを反映する。
void SceneLoading::Render(float deltaTime_)
{
    BS_PROFILE_SCOPE("SceneLoading::Render");
    SceneBase::Render(deltaTime_);
}

// ロード状態の操作UIも配置ComponentのRenderUIから提出する。
void SceneLoading::DrawGUI() {}
