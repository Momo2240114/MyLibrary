#pragma once

#include "../Window/EditorWindow.h"

class AnimationControllerComponent;
class GltfModelCommponennt;

class AnimNodeEditorWindow : public EditorWindow
{
public:
    AnimNodeEditorWindow(GameObjectManager* manager)
        : EditorWindow("Animation Node Graph", manager)
    {
        isVisible = false;
    }

    void Draw() override;

private:
    bool ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const;
};

