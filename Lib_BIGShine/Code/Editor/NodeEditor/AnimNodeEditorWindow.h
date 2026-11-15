#pragma once

#include "Editor/Workspace/EditorWorkspaceWindow.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Engine/Graphics/Resource/Animation/AnimationState.h"

class AnimationControllerComponent;

class AnimNodeEditorWindow : public EditorWorkspaceWindow
{
public:
    AnimNodeEditorWindow(GameObjectManager* manager_)
        : EditorWorkspaceWindow("Animation Node Graph", manager_),
        assetBrowser(manager_)
    {
        isVisible = false;
    }

    void Draw() override;

private:

    void DrawScenePreviewPanel();
    void DrawSceneHierarchyPanel();
    void DrawSceneInspectorPanel();
    void DrawGraphPanel(AnimationControllerComponent* animController_);
    void DrawAssetBrowserPanel();

    AssetBrowserWindow assetBrowser;
    // 右Inspector、プレビュー、階層、AssetBrowserなどの分割サイズを保持する。
    float rightPanelWidth = 520.0f;
    float scenePreviewHeight = 420.0f;
    float hierarchyWidth = 260.0f;
    float assetBrowserHeight = 260.0f;
    float transitionInspectorWidth = 320.0f;
    // 新旧接続方式を切り替えて、既存グラフとの互換を保ちながら編集できるようにする。
    ANIM_GRAPH_CONNECTION_MODE graphConnectionMode = ANIM_GRAPH_CONNECTION_MODE::LEGACY_TWO_PIN;
    bool layoutSizeInitialized = false;
};

