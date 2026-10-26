#pragma once

#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>
#include "LoadingTypes.h"
#include <cstdint>

class Scene;

class SceneManager
{
public:
    // 初期化
    static void Initialize(std::unique_ptr<Scene> firstScene);

    // 更新・描画
    static void Update(float elapsedTime);
    static void Render(float elapsedTime);
    static void DrawGUI();
    // 次Sceneを一度描画した後、共通ManagerへRevealを通知する。
    static void RenderTransition();
    static bool IsTransitionActive();
    static bool IsTransitionInputBlocked();

    // シーン切り替え予約
    static void ChangeScene(std::unique_ptr<Scene> scene, bool callInitialize = true);

    // 現在のシーン取得
    static Scene* GetCurrentScene();
    // UIは主スレッドから値を取得し、非同期読込の共有データを直接参照しない。
    static LoadingStatus GetLoadingStatus();
    // 実行中にSceneが切り替わっても、開始元の編集内容を保持する。
    static void BeginEditorPlaySession(const nlohmann::json& snapshot_);
    static void EndEditorPlaySession();
    static bool IsEditorPlaySessionActive();
    static const nlohmann::json& GetEditorPlaySnapshot();
    static bool IsRestoringEditorScene();
    static void RequestReturnToEditor();

    // 現在のシーンクリア 
    static void Clear();

private:
    static std::atomic<bool> editorPlaySessionActive;
    static std::atomic<bool> restoringEditorScene;
    static nlohmann::json editorPlaySnapshot;
    static std::unique_ptr<Scene> NowScene;
    static std::unique_ptr<Scene> NextScene;

    static bool  callInitOnChange;
    static std::uint64_t transitionHandle;
    static std::string transitionError;
    static bool initializationFailed;

};
