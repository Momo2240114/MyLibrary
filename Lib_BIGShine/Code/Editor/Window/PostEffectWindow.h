#pragma once

#include "EditorWindow.h"

// GraphicsDeviceManagerが持つPostEffect設定をEditorから操作するWindow。
class PostEffectWindow : public EditorWindow
{
public:
    explicit PostEffectWindow(GameObjectManager* manager_)
        : EditorWindow("Post Effect", manager_)
    {
    }

    void Draw() override;
};
