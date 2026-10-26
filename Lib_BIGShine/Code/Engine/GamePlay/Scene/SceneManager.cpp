#include "PrecompiledHeader.h"

#include "SceneManager.h"
#include "Engine/UI/ScreenTransitionManager.h"
#include "IScene.h"
#include "SceneBase.h"
#include "SceneGame.h"
#include "SceneLoading.h"
#include "SceneTitle.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include <imgui.h>
#include "Engine/Audio/AudioManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

std::unique_ptr<Scene> SceneManager::NowScene = nullptr;
std::unique_ptr<Scene> SceneManager::NextScene = nullptr;
bool SceneManager::callInitOnChange;
TransitionHandle SceneManager::transitionHandle = 0;
std::string SceneManager::transitionError;
bool SceneManager::initializationFailed = false;
std::atomic<bool> SceneManager::editorPlaySessionActive{false};
std::atomic<bool> SceneManager::restoringEditorScene{false};
nlohmann::json SceneManager::editorPlaySnapshot;

void SceneManager::Initialize(std::unique_ptr<Scene> firstScene)
{
    // 所有権を NowScene に渡す
    NowScene = std::move(firstScene);

    // もし GameObjectManager::ChangeScene が「現在のシーン名」を記録するだけのものなら以下のように。
    if (NowScene)
    {
        // 先に初期化を行い、固有の GameObjectManager を産み落とす
        NowScene->Initialize();

        // 誕生したマネージャーに対し、現在のシーンを認識させる
        SceneBase* sceneBase_ = dynamic_cast<SceneBase*>(NowScene.get());
        if (sceneBase_ && sceneBase_->GetGameObjectManager())
        {
            sceneBase_->GetGameObjectManager()->ChangeScene(sceneBase_);
        }
    }

}

// Sceneを更新しながら、完全被覆を描画した次のフレームでのみ所有権を移す。
void SceneManager::Update(float elapsedTime)
{
    BS_PROFILE_SCOPE("SceneManager::TransitionUpdate");
    bool deferTransition_ = false;
    if ((NextScene || IsTransitionActive()) && editorPlaySessionActive)
        if (const auto source_ = dynamic_cast<SceneBase*>(NowScene.get()))
            if (const auto manager_ = source_->GetGameObjectManager())
            {
                if (manager_->IsEditorStopRequested()) RequestReturnToEditor();
                else deferTransition_ = manager_->GetEditorPlayMode() == EditorPlayMode::PAUSED || manager_->IsEditorPauseRequested();
            }
    auto& transition_ = ScreenTransitionManager::Instance();
    if (NextScene && !deferTransition_ && !IsTransitionActive() && transition_.IsCovered(transition_.GetHandle()) && !transition_.IsPreview())
        transitionHandle = transition_.RetainCovered();
    if (NextScene && !deferTransition_ && !transition_.IsActive())
    {
        const auto source_ = dynamic_cast<SceneBase*>(NowScene.get());
        const auto manager_ = source_ ? source_->GetGameObjectManager() : nullptr;
        const auto loading_ = dynamic_cast<SceneLoading*>(NowScene.get());
        const bool engine_ = loading_ ? loading_->IsEnginePresentation() : (manager_ && manager_->IsEngineLoadingPresentation()) || !GameRuntimeSettings::IsGameOnly() && !editorPlaySessionActive &&
            (!manager_ || manager_->GetEditorPlayMode() == EditorPlayMode::EDITOR) && !dynamic_cast<SceneTitle*>(NowScene.get());
        const auto sceneName_ = manager_ ? manager_->GetLoadedSceneName() : source_ ? source_->GetTransitionSceneName() : (NowScene ? NowScene->GetSceneName() : std::string());
        transitionHandle = transition_.Request(transition_.GetProfile(engine_ ? "BigShine" : "ANATHEMA",sceneName_),TransitionPlayback::COVER,{},{},{},false,true,100);
    }
    if (NextScene && !deferTransition_ && transition_.IsCovered(transitionHandle))
    {
        auto nextScene_ = std::move(NextScene);
        const bool initialize_ = callInitOnChange;
        transitionError.clear();
        initializationFailed = false;
        try
        {
            // 旧SceneのPhysX等を先に解放する順序は維持し、遷移用GPUリソースだけを残す。
            if (NowScene) NowScene->Finalize();
            NowScene.reset();
            NextScene.reset();
            NowScene = std::move(nextScene_);
            if (NowScene && initialize_) NowScene->Initialize();
            if (restoringEditorScene) EndEditorPlaySession();
            if (const auto sceneBase_ = dynamic_cast<SceneBase*>(NowScene.get()))
                if (const auto manager_ = sceneBase_->GetGameObjectManager()) manager_->ChangeScene(sceneBase_);
        }
        catch (const std::exception& exception_)
        {
            transitionError = exception_.what();
            initializationFailed = true;
        }
        catch (...)
        {
            transitionError = "Scene initialization failed.";
            initializationFailed = true;
        }
        transition_.Hold(transitionHandle);
    }
    if (NowScene && !initializationFailed) NowScene->Update(elapsedTime);
}
void SceneManager::Render(float elapsedTime)
{
    if (NowScene && !initializationFailed)
    {
        NowScene->Render(elapsedTime);
    }
}

void SceneManager::DrawGUI()
{
    if (NowScene && !initializationFailed) NowScene->DrawGUI();
    if (!transitionError.empty())
    {
        if (ImGui::Begin("Scene transition error"))
        {
            ImGui::TextWrapped("%s", transitionError.c_str());
            if (!IsTransitionActive() && ImGui::Button("Return to Title"))
                ChangeScene(std::make_unique<SceneLoading>(std::make_unique<SceneTitle>()));
            if (IsEditorPlaySessionActive() && ImGui::Button("Return to Editor")) RequestReturnToEditor();
        }
        ImGui::End();
    }
}

// 初回描画が終わっても、非同期初期化中は被覆した状態を維持する。
void SceneManager::RenderTransition()
{
    bool ready_ = NowScene != nullptr;
    bool failed_ = initializationFailed;
    if (const auto source_ = dynamic_cast<SceneBase*>(NowScene.get()))
        if (const auto manager_ = source_->GetGameObjectManager())
        {
            ready_ = manager_->GetSceneRuntimeState() == SCENE_RUNTIME_STATE::RUNNING;
            failed_ = failed_ || manager_->GetSceneRuntimeState() == SCENE_RUNTIME_STATE::FAILED;
        }
    auto& transition_ = ScreenTransitionManager::Instance();
    if (transition_.Owns(transitionHandle) && transition_.GetState() == TransitionState::WAITING_FOR_SCENE && (ready_ || failed_ || !NowScene)) transition_.Reveal(transitionHandle);
}

bool SceneManager::IsTransitionActive() { const auto& transition_ = ScreenTransitionManager::Instance(); return transition_.Owns(transitionHandle) && transition_.IsActive(); }
bool SceneManager::IsTransitionInputBlocked() { return ScreenTransitionManager::Instance().IsInputBlocked(); }
void SceneManager::ChangeScene(std::unique_ptr<Scene> scene, bool callInitialize)
{
    // 試写中の完了Actionで編集Sceneを離れず、WorkspaceのStopで必ず復元できるようにする。
    if (const auto source_ = dynamic_cast<SceneBase*>(NowScene.get()))
        if (const auto manager_ = source_->GetGameObjectManager(); manager_ && manager_->IsCinematicSequencePreview()) return;
    if (!scene) return;
    // 最初の予約を保持する。ロード完了は開き演出中でも1件だけ受け取り、終了後に閉じる。
    if (!restoringEditorScene && (NextScene || (IsTransitionActive() && !dynamic_cast<SceneLoading*>(NowScene.get())))) return;
    if (ScreenTransitionManager::Instance().IsPreview()) ScreenTransitionManager::Instance().Cancel(ScreenTransitionManager::Instance().GetHandle());
    // 同一フレームのロード要求は最初の1件だけ受け付ける。Stopによる復元は従来経路を使う。
    if (dynamic_cast<SceneLoading*>(scene.get()) &&
        (NextScene || (dynamic_cast<SceneLoading*>(NowScene.get()) && !initializationFailed))) return;
    // 新しいシーンを予約（所有権を NextScene が持つ）
    NextScene = std::move(scene);
    callInitOnChange = callInitialize;
}

void SceneManager::Clear()
{
    ScreenTransitionManager::Instance().Cancel(transitionHandle);
    transitionHandle = 0;
    transitionError.clear();
    initializationFailed = false;
    // ロード済みSceneの移譲を取り消す場合も、専用の終了処理を通して解放する。
    if (NextScene && !callInitOnChange) NextScene->Finalize();
    if (NowScene)
    {
        NowScene->Finalize();
    }

    // PhysXなどの基盤が終了する前に、Sceneが所有するComponentを破棄する。
    NowScene.reset();
    NextScene.reset();
}

Scene* SceneManager::GetCurrentScene()
{
    return NowScene.get();
}


void SceneManager::BeginEditorPlaySession(const nlohmann::json& snapshot_)
{
    BS_PROFILE_SCOPE("EditorPlayMode::BeginSession");
    editorPlaySnapshot = snapshot_;
    restoringEditorScene = false;
    editorPlaySessionActive = true;
}

void SceneManager::EndEditorPlaySession()
{
    BS_PROFILE_SCOPE("EditorPlayMode::EndSession");
    editorPlaySessionActive = false;
    restoringEditorScene = false;
    editorPlaySnapshot = nlohmann::json();
    NextScene.reset();
    AudioManager::Instance().SetEditorPlayPaused(false);
}

bool SceneManager::IsEditorPlaySessionActive() { return editorPlaySessionActive.load(); }
bool SceneManager::IsRestoringEditorScene() { return restoringEditorScene.load(); }
const nlohmann::json& SceneManager::GetEditorPlaySnapshot() { return editorPlaySnapshot; }

void SceneManager::RequestReturnToEditor()
{
    BS_PROFILE_SCOPE("EditorPlayMode::ReturnToEditor");
    if (editorPlaySnapshot.is_null()) return;
    // 読込スレッドはSceneLoadingのFinalizeで終了を待ち、退避データは復元完了まで保持する。
    ScreenTransitionManager::Instance().Cancel(ScreenTransitionManager::Instance().GetHandle());
    transitionHandle = 0;
    if (NextScene && !callInitOnChange) NextScene->Finalize();
    NextScene.reset();
    restoringEditorScene = true;
    editorPlaySessionActive = false;
    AudioManager::Instance().SetEditorPlayPaused(false);
    AudioManager::Instance().StopBGM();
    AudioManager::Instance().StopSE();
    ChangeScene(std::make_unique<SceneGame>());
}


LoadingStatus SceneManager::GetLoadingStatus()
{
    if (const auto loading_ = dynamic_cast<SceneLoading*>(NowScene.get())) return loading_->GetLoadingStatus();
    return {};
}
