#pragma once

#include "Editor/Workspace/EditorWorkspaceWindow.h"
#include "Editor/Window/AssetBrowserWindow.h"

class AnimationControllerComponent;
class GltfModelCommponennt;

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


    bool ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const;
    void DrawScenePreviewPanel();
    void DrawSceneHierarchyPanel();
    void DrawSceneInspectorPanel();
    void DrawGraphPanel(AnimationControllerComponent* animController_);
    void DrawAssetBrowserPanel();

    AssetBrowserWindow assetBrowser;
    float rightPanelWidth = 520.0f;
    float scenePreviewHeight = 420.0f;
    float hierarchyWidth = 260.0f;
    float assetBrowserHeight = 260.0f;
    bool layoutSizeInitialized = false;
};

