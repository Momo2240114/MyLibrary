#pragma once

// EditorWindowManager.h
#include <vector>
#include <memory>
#include <string>
#include "EditorWindow.h" 
#include "Editor/EditorCameraData.h"

class GameObjectManager;

class EditorWindowManager
{
public:
    // コンストラクタでObjectManagerを入れる
    EditorWindowManager(GameObjectManager* manager) : gameObjectManager(manager) {}

    void AddWindow(std::unique_ptr<EditorWindow> window) {
        windows.push_back(std::move(window));
    }

    void Update(float deltaTime_);
    void DrawAll();
    bool IsWorkspaceMode() const { return activeWorkspaceIndex >= 0 && activeWorkspaceIndex < static_cast<int>(windows.size()) && windows[activeWorkspaceIndex]->IsVisible(); }
    bool ShouldPauseScene() const { return IsWorkspaceMode() && windows[activeWorkspaceIndex]->ShouldPauseScene(); }
    bool ShouldRenderSceneWhenPaused() const { return IsWorkspaceMode() && windows[activeWorkspaceIndex]->ShouldRenderSceneWhenPaused(); }
    bool ShouldUseDockedSceneView() const { return useDockedSceneView || (IsWorkspaceMode() && windows[activeWorkspaceIndex]->RequiresSceneRenderTarget()); }
private:
    void DrawMenuBar();
    // 配布用EXEを選び、新規フォルダーへのゲーム書き出し結果を表示する。
    void DrawGameExport();
    bool showGameExport = false;
    std::string gameExportStatus;
    const char* GetWindowMenuCategory(const EditorWindow& window_) const;
    void DrawWorkspaceSelectorBar();
    void SetActiveWorkspace(int workspaceIndex_);
    std::string GetActiveWorkspaceName() const;
    float GetWorkspaceSelectorHeight() const;
    void DrawSceneEditorDockSpace();
    void BuildDefaultDockLayout(ImGuiID dockspaceId_, const ImVec2& dockspaceSize_);
    void DrawSceneView();
    // Sceneビュー上部中央へPlay/Pause/Stopを配置する。
    void DrawPlayModeToolbar();
    EditorCameraData sceneEditorCameraData;
    bool sceneEditorCameraCaptured = false;

    ImGuiID sceneDockId = 0;
    ImGuiID leftDockId = 0;
    ImGuiID rightDockId = 0;
    ImGuiID bottomDockId = 0;
    bool useDockedSceneView = true;
    int activeWorkspaceIndex = -1;
    GameObjectManager* gameObjectManager; // ここが繋がる！
    std::vector<std::unique_ptr<EditorWindow>> windows;
};

