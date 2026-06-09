#pragma once

#include "Editor/Workspace/EditorWorkspaceWindow.h"

class AnimationControllerComponent;
class GltfModelCommponennt;

class AnimNodeEditorWindow : public EditorWorkspaceWindow
{
public:
    AnimNodeEditorWindow(GameObjectManager* manager)
        : EditorWorkspaceWindow("Animation Node Graph", manager)
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
    void DrawStatePanel(AnimationControllerComponent* animController_);
};

