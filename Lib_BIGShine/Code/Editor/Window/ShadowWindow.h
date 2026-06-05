#pragma once

#include "EditorWindow.h" 

class ModelRenderSystem;

class ShadowWindow : public EditorWindow
{
public:
    ShadowWindow(const std::string& name, GameObjectManager* manager, ModelRenderSystem& renderSystem)
        : EditorWindow("Shadow", manager), modelRenderSystem(renderSystem)
    {
    }

    void Draw() override;

private:
    ModelRenderSystem& modelRenderSystem;
};
