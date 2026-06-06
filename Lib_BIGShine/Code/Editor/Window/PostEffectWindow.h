#pragma once

#include "EditorWindow.h"

class PostEffectWindow : public EditorWindow
{
public:
    explicit PostEffectWindow(GameObjectManager* manager_)
        : EditorWindow("Post Effect", manager_)
    {
    }

    void Draw() override;
};
