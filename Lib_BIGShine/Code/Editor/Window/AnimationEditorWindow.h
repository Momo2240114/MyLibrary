#pragma once

#include "EditorWindow.h"

class AnimationControllerComponent;
class GltfModelCommponennt;

class AnimationEditorWindow : public EditorWindow
{
public:
    AnimationEditorWindow(GameObjectManager* manager)
        : EditorWindow("Animation Sequence", manager)
    {
        isVisible = false;
    }

    void Draw() override;

private:
    bool ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const;
};

